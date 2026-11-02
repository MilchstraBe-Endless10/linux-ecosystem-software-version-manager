#pragma once

#include "version_id.hpp"

#include <string>
#include <vector>

namespace version_id_001::fixtures {

struct ParseCase {
    const char *id;
    VersionDescriptor input;
    ParseStatus expected;
};

inline const std::vector<ParseCase> parseCases{
    {"npm stable SemVer", {"npm", "1.2.3", VersionScheme::semver}, ParseStatus::valid},
    {"pnpm prerelease and build metadata", {"pnpm", "2.0.0-rc.1+ci.7", VersionScheme::semver}, ParseStatus::valid},
    {"npm core leading zero", {"npm", "01.2.3", VersionScheme::semver}, ParseStatus::invalid},
    {"npm missing patch", {"npm", "1.2", VersionScheme::semver}, ParseStatus::invalid},
    {"pipx PEP 440", {"pipx", "1.2.0rc1", VersionScheme::pep440}, ParseStatus::unsupported},
    {"uv PEP 440", {"uv", "2.4.0.post1", VersionScheme::pep440}, ParseStatus::unsupported},
    {"mise without declared scheme", {"mise", "2025.08", VersionScheme::unknown}, ParseStatus::unsupported},
    {"unknown source", {"unknown", "latest", VersionScheme::unknown}, ParseStatus::unsupported},
};

struct CompareCase {
    const char *id;
    VersionDescriptor left;
    VersionDescriptor right;
    Comparison expected;
};

inline const std::vector<CompareCase> compareCases{
    {"npm ascending patch", {"npm", "1.2.3", VersionScheme::semver}, {"npm", "1.2.4", VersionScheme::semver}, Comparison::less},
    {"pnpm prerelease before release", {"pnpm", "1.2.3-rc.9", VersionScheme::semver}, {"pnpm", "1.2.3", VersionScheme::semver}, Comparison::less},
    {"build metadata ignored", {"npm", "1.2.3+one", VersionScheme::semver}, {"pnpm", "1.2.3+two", VersionScheme::semver}, Comparison::equal},
    {"cross-manager explicit SemVer", {"npm", "2.1.0", VersionScheme::semver}, {"pnpm", "2.2.0", VersionScheme::semver}, Comparison::less},
    {"pipx and uv are not treated as SemVer", {"pipx", "1.2.0", VersionScheme::pep440}, {"uv", "1.3.0", VersionScheme::pep440}, Comparison::incomparable},
    {"mise scheme is unknown", {"mise", "1.2.3", VersionScheme::unknown}, {"npm", "1.2.4", VersionScheme::semver}, Comparison::incomparable},
    {"invalid SemVer is incomparable", {"npm", "1.02.3", VersionScheme::semver}, {"npm", "1.3.0", VersionScheme::semver}, Comparison::incomparable},
};

struct IdentityPairCase {
    const char *id;
    InstallationInstanceDescriptor left;
    InstallationInstanceDescriptor right;
    bool expectSameKey;
};

inline const std::vector<IdentityPairCase> identityPairCases{
    {"same normalized identity", {"NPM", "USER", "/sandbox/user", "stable", "express", ""}, {"npm", "user", "/sandbox/user", "stable", "express", ""}, true},
    {"different install roots stay distinct", {"npm", "user", "/sandbox/user-a", "stable", "express", ""}, {"npm", "user", "/sandbox/user-b", "stable", "express", ""}, false},
    {"different channels stay distinct", {"mise", "project", "/sandbox/project", "stable", "node", ""}, {"mise", "project", "/sandbox/project", "nightly", "node", ""}, false},
    {"different managers stay distinct", {"npm", "user", "/sandbox/user", "stable", "express", ""}, {"pnpm", "user", "/sandbox/user", "stable", "express", ""}, false},
    {"different scopes stay distinct", {"uv", "project", "/sandbox/project", "", "ruff", ""}, {"uv", "user", "/sandbox/project", "", "ruff", ""}, false},
    {"different package IDs stay distinct", {"pipx", "user", "/sandbox/user", "", "tool-a", ""}, {"pipx", "user", "/sandbox/user", "", "tool-b", ""}, false},
    {"different qualifiers stay distinct", {"mise", "project", "/sandbox/project", "stable", "node", "runtime-a"}, {"mise", "project", "/sandbox/project", "stable", "node", "runtime-b"}, false},
    {"empty channel differs from stable", {"npm", "user", "/sandbox/user", "", "express", ""}, {"npm", "user", "/sandbox/user", "stable", "express", ""}, false},
    {"delimiters in fields cannot collide", {"npm", "user", "/sandbox/root", "stable", "a:b", ""}, {"npm", "user", "/sandbox/root", "stable", "a", "b:"}, false},
};

struct InvalidIdentityCase {
    const char *id;
    InstallationInstanceDescriptor input;
    IdentityError expected;
};

inline const std::vector<InvalidIdentityCase> invalidIdentityCases{
    {"missing manager", {"", "user", "/sandbox/user", "", "express", ""}, IdentityError::missingManager},
    {"missing scope", {"npm", "", "/sandbox/user", "", "express", ""}, IdentityError::missingScope},
    {"missing install root", {"npm", "user", "", "", "express", ""}, IdentityError::missingInstallRoot},
    {"missing package ID", {"npm", "user", "/sandbox/user", "", "", ""}, IdentityError::missingPackageId},
};

} // namespace version_id_001::fixtures
