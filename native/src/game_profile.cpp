#include "champions/game_profile.hpp"

#include <algorithm>

namespace champions {
namespace {
char lowerHex(char c) {
    return c >= 'A' && c <= 'F' ? static_cast<char>(c - 'A' + 'a') : c;
}
bool isSha256(std::string_view digest) {
    return digest.size() == 64 && std::all_of(digest.begin(), digest.end(), [](char c) {
        c = lowerHex(c);
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
} // namespace

const std::array<GameProfile, 2>& gameProfiles() {
    // Identity: champions-return-to-arms/docs/REBUILD.md, baseline section.
    // Format subsets: this repository's TEXTURE_PREVIEW.md / MODEL_FORMS.md.
    static const std::array<GameProfile, 2> profiles{{
        {"rta-us-v2.00", GameTitle::ReturnToArms, "Champions: Return to Arms",
         "US", "v2.00", ProfileValidation::DocumentedBaseline,
         ContentFingerprint{FingerprintScope::Executable, FingerprintAlgorithm::Sha256,
             "292f6f48e3609c0026def2307d76c222cd5b389303d53b89b0e107d7b1dcd675"},
         {SupportedFormat::GobArchive, SupportedFormat::LmpArchive,
          SupportedFormat::RtaTexture32, SupportedFormat::RtaTexture4,
          SupportedFormat::RtaTexture8, SupportedFormat::RtaModel42,
          SupportedFormat::RtaModel44}},
        {"norrath-unvalidated", GameTitle::Norrath, "Champions of Norrath",
         "", "", ProfileValidation::Unvalidated, std::nullopt, {}}
    }};
    return profiles;
}

const GameProfile* findGameProfile(std::string_view id) {
    for (const auto& profile : gameProfiles()) {
        if (profile.id == id) return &profile;
    }
    return nullptr;
}

ProfileCheck checkContentFingerprint(const GameProfile& profile,
                                     const ContentFingerprint& supplied) {
    const std::string name(profile.display_name);
    if (profile.validation != ProfileValidation::DocumentedBaseline ||
        profile.title == GameTitle::Norrath) {
        return {ProfileCheckCode::UnvalidatedProfile,
                name + " is unvalidated; decoding is refused until its own revision, baseline and format contracts are established."};
    }
    if (!profile.baseline) {
        return {ProfileCheckCode::MissingBaseline, name + " has no documented baseline fingerprint; decoding is refused."};
    }
    const auto& baseline = *profile.baseline;
    if (supplied.scope != baseline.scope) {
        return {ProfileCheckCode::WrongScope,
                "Supply the selected baseline executable fingerprint; an archive or asset fingerprint cannot establish this profile."};
    }
    if (supplied.algorithm != baseline.algorithm ||
        baseline.algorithm != FingerprintAlgorithm::Sha256) {
        return {ProfileCheckCode::WrongAlgorithm, "This profile requires a SHA-256 executable fingerprint."};
    }
    if (!isSha256(supplied.digest) || !isSha256(baseline.digest)) {
        return {ProfileCheckCode::MalformedDigest, "SHA-256 must contain exactly 64 hexadecimal characters."};
    }
    if (!std::equal(supplied.digest.begin(), supplied.digest.end(), baseline.digest.begin(),
                    [](char a, char b) { return lowerHex(a) == lowerHex(b); })) {
        return {ProfileCheckCode::UnknownFingerprint,
                "Unknown executable fingerprint for " + name + " " + std::string(profile.region) +
                " " + std::string(profile.revision) + "; decoding is refused. Select a documented revision; archive listing success is not title detection."};
    }
    return {ProfileCheckCode::Accepted, "Documented executable baseline matched; individual content still requires format validation."};
}

ProfileCheck checkProfileDecoding(std::string_view profile_id,
                                 const ContentFingerprint& supplied,
                                 SupportedFormat format) {
    const auto* profile = findGameProfile(profile_id);
    if (!profile) {
        return {ProfileCheckCode::UnknownProfile, "Unknown profile ID; explicitly select a documented title/revision before decoding."};
    }
    auto check = checkContentFingerprint(*profile, supplied);
    if (!check.accepted()) return check;
    if (std::find(profile->supported_formats.begin(), profile->supported_formats.end(), format) ==
        profile->supported_formats.end()) {
        return {ProfileCheckCode::UnsupportedFormat, "The selected profile does not support this format; decoding is refused."};
    }
    return check;
}
} // namespace champions
