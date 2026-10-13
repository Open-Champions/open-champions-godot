#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace champions {

enum class GameTitle { ReturnToArms, Norrath };
enum class ProfileValidation { DocumentedBaseline, Unvalidated };
enum class FingerprintScope { Executable, Archive, Asset };
enum class FingerprintAlgorithm { Sha256 };
enum class SupportedFormat {
    GobArchive, LmpArchive, RtaTexture32, RtaTexture4, RtaTexture8,
    RtaModel42, RtaModel44
};

// Digest text is exactly 64 hexadecimal characters for SHA-256. No file access
// or hashing is performed here; callers must hash the selected executable.
struct ContentFingerprint {
    FingerprintScope scope;
    FingerprintAlgorithm algorithm;
    std::string_view digest;
};

struct GameProfile {
    std::string_view id;
    GameTitle title;
    std::string_view display_name;
    std::string_view region;   // Empty means unknown, never a default region.
    std::string_view revision; // Empty means unknown.
    ProfileValidation validation;
    std::optional<ContentFingerprint> baseline;
    std::vector<SupportedFormat> supported_formats;
};

// Registry views have process lifetime. Selection is by exact explicit ID.
const std::array<GameProfile, 2>& gameProfiles();
const GameProfile* findGameProfile(std::string_view id);

enum class ProfileCheckCode {
    Accepted, UnknownProfile, UnvalidatedProfile, MissingBaseline,
    WrongScope, WrongAlgorithm, MalformedDigest, UnknownFingerprint,
    UnsupportedFormat
};
struct ProfileCheck {
    ProfileCheckCode code;
    std::string message;
    bool accepted() const noexcept { return code == ProfileCheckCode::Accepted; }
};

// Matching identifies only the documented baseline, not the provenance or
// semantic validity of other content. Never select a title from parser success.
ProfileCheck checkContentFingerprint(const GameProfile& profile,
                                     const ContentFingerprint& supplied);
// Resolves a registry ID, checks identity, then checks the bounded decoder subset.
ProfileCheck checkProfileDecoding(std::string_view profile_id,
                                 const ContentFingerprint& supplied,
                                 SupportedFormat format);

} // namespace champions
