#pragma once

#include <string>
#include <vector>
#include <optional>

namespace version_id_001 {

enum class VersionScheme { semver, pep440, opaque, unknown };
enum class ParseStatus { valid, invalid, unsupported };
enum class Comparison { less, equal, greater, incomparable };

struct VersionDescriptor {
    std::string manager;
    std::string raw;
    VersionScheme scheme = VersionScheme::unknown;
};

struct SemanticVersion {
    std::string major;
    std::string minor;
    std::string patch;
    std::vector<std::string> prerelease;
    std::vector<std::string> build;
};

struct ParseResult {
    ParseStatus status = ParseStatus::invalid;
    SemanticVersion version;
};

ParseResult parseVersion(const VersionDescriptor& descriptor);
Comparison compareVersions(const VersionDescriptor& left, const VersionDescriptor& right);

struct InstallationInstanceDescriptor {
    std::string manager;
    std::string scope;
    std::string installRoot;
    std::string channel;
    std::string packageId;
    std::string instanceQualifier;
};

enum class IdentityError { none, missingManager, missingScope, missingInstallRoot, missingPackageId };

struct IdentityResult {
    std::optional<std::string> key;
    IdentityError error = IdentityError::none;
};

IdentityResult makeInstallationInstanceKey(const InstallationInstanceDescriptor& descriptor);

} // namespace version_id_001