#include "champions/game_profile.hpp"
#include "champions/archive.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
void code(const champions::ProfileCheck& result, champions::ProfileCheckCode expected) {
    require(result.code == expected, "unexpected profile result code");
    require(!result.message.empty(), "missing diagnostic");
    require(result.accepted() == (expected == champions::ProfileCheckCode::Accepted),
            "acceptance disagrees with result code");
}
} // namespace

int main() {
    using namespace champions;
    try {
        const auto& profiles = gameProfiles();
        require(profiles.size() == 2, "registry size");
        const auto* rta = findGameProfile("rta-us-v2.00");
        const auto* norrath = findGameProfile("norrath-unvalidated");
        require(rta && norrath && rta != norrath, "explicit distinct profiles");
        require(rta->title == GameTitle::ReturnToArms && rta->region == "US" &&
                rta->revision == "v2.00", "documented RTA identity");
        require(rta->baseline.has_value(), "RTA baseline");
        require(rta->baseline->digest ==
                "292f6f48e3609c0026def2307d76c222cd5b389303d53b89b0e107d7b1dcd675",
                "documented executable digest");
        require(norrath->region.empty() && norrath->revision.empty() &&
                !norrath->baseline && norrath->supported_formats.empty() &&
                norrath->validation == ProfileValidation::Unvalidated, "no invented Norrath facts");
        for (auto id : {"", "rta", "RTA-US-V2.00", "norrath", "unknown"}) {
            require(findGameProfile(id) == nullptr, "no automatic or alias selection");
            code(checkProfileDecoding(id, *rta->baseline, SupportedFormat::GobArchive),
                 ProfileCheckCode::UnknownProfile);
        }
        code(checkContentFingerprint(*rta, *rta->baseline), ProfileCheckCode::Accepted);
        std::string uppercase(rta->baseline->digest);
        for (auto& c : uppercase) if (c >= 'a' && c <= 'f') c = static_cast<char>(c - 'a' + 'A');
        auto supplied = *rta->baseline;
        supplied.digest = uppercase;
        code(checkContentFingerprint(*rta, supplied), ProfileCheckCode::Accepted);
        for (auto scope : {FingerprintScope::Archive, FingerprintScope::Asset}) {
            supplied = *rta->baseline;
            supplied.scope = scope;
            code(checkContentFingerprint(*rta, supplied), ProfileCheckCode::WrongScope);
        }
        supplied = *rta->baseline;
        supplied.algorithm = static_cast<FingerprintAlgorithm>(999);
        code(checkContentFingerprint(*rta, supplied), ProfileCheckCode::WrongAlgorithm);
        for (const std::string digest : {std::string(), std::string(63, '0'),
                std::string(65, '0'), std::string(64, 'g'), std::string(64, ' '),
                std::string(64, '\0')}) {
            supplied = *rta->baseline;
            supplied.digest = digest;
            code(checkContentFingerprint(*rta, supplied), ProfileCheckCode::MalformedDigest);
        }
        std::string unknown(64, '0');
        supplied = *rta->baseline;
        supplied.digest = unknown;
        const auto refusal = checkContentFingerprint(*rta, supplied);
        code(refusal, ProfileCheckCode::UnknownFingerprint);
        require(refusal.message.find("US v2.00") != std::string::npos,
                "refusal identifies requested revision");
        require(rta->supported_formats.size() == 7, "bounded format inventory");
        for (auto format : rta->supported_formats) {
            code(checkProfileDecoding(rta->id, *rta->baseline, format), ProfileCheckCode::Accepted);
            code(checkProfileDecoding(rta->id, supplied, format), ProfileCheckCode::UnknownFingerprint);
            code(checkProfileDecoding(norrath->id, *rta->baseline, format), ProfileCheckCode::UnvalidatedProfile);
        }
        code(checkProfileDecoding(rta->id, *rta->baseline, static_cast<SupportedFormat>(999)),
             ProfileCheckCode::UnsupportedFormat);
        auto missing = *rta;
        missing.baseline.reset();
        code(checkContentFingerprint(missing, supplied), ProfileCheckCode::MissingBaseline);
        auto unvalidated = *rta;
        unvalidated.validation = ProfileValidation::Unvalidated;
        code(checkContentFingerprint(unvalidated, *rta->baseline), ProfileCheckCode::UnvalidatedProfile);
        // Even a caller-supplied copy cannot enable decoding for Norrath.
        auto pretend_norrath = *rta;
        pretend_norrath.title = GameTitle::Norrath;
        code(checkContentFingerprint(pretend_norrath, *rta->baseline), ProfileCheckCode::UnvalidatedProfile);
        // An authored empty LMP can list successfully without establishing identity.
        const std::uint8_t empty_lmp[] = {0, 0, 0, 0};
        require(readLmp({empty_lmp, sizeof(empty_lmp)}).empty(), "synthetic archive lists");
        code(checkProfileDecoding(rta->id, supplied, SupportedFormat::LmpArchive),
             ProfileCheckCode::UnknownFingerprint);
        code(checkProfileDecoding(norrath->id, supplied, SupportedFormat::LmpArchive),
             ProfileCheckCode::UnvalidatedProfile);
        std::cout << checks << " game-profile checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
