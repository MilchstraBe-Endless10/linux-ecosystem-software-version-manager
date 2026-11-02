#include "version_id.hpp"
#include "test_fixtures.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace version_id_001;
using namespace version_id_001::fixtures;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; failures++; } \
} while (0)

int main() {
    // Parse cases
    for (const auto& c : parseCases) {
        ParseResult r = parseVersion(c.input);
        CHECK(r.status == c.expected, std::string(c.id) + " parse status");
    }

    // Compare cases
    for (const auto& c : compareCases) {
        Comparison r = compareVersions(c.left, c.right);
        CHECK(r == c.expected, std::string(c.id) + " compare");
    }

    // Identity pair cases
    for (const auto& c : identityPairCases) {
        IdentityResult l = makeInstallationInstanceKey(c.left);
        IdentityResult r = makeInstallationInstanceKey(c.right);
        CHECK(l.error == IdentityError::none, std::string(c.id) + " left valid");
        CHECK(r.error == IdentityError::none, std::string(c.id) + " right valid");
        if (l.key && r.key) {
            bool same = (*l.key == *r.key);
            CHECK(same == c.expectSameKey, std::string(c.id) + " key equality");
        }
    }

    // Invalid identity cases
    for (const auto& c : invalidIdentityCases) {
        IdentityResult r = makeInstallationInstanceKey(c.input);
        CHECK(r.error == c.expected, std::string(c.id) + " error");
        CHECK(!r.key.has_value(), std::string(c.id) + " no key");
    }

    // Deterministic repeatability
    {
        InstallationInstanceDescriptor d{"npm","user","/root","stable","pkg","q"};
        IdentityResult a = makeInstallationInstanceKey(d);
        IdentityResult b = makeInstallationInstanceKey(d);
        CHECK(a.key == b.key, "deterministic key");
    }

    // Large numeric identifiers no overflow
    {
        VersionDescriptor a{"x","999999999999999999999.0.0",VersionScheme::semver};
        VersionDescriptor b{"x","1000000000000000000000.0.0",VersionScheme::semver};
        CHECK(compareVersions(a,b) == Comparison::less, "large numeric compare");
    }

    // Prerelease numeric vs non-numeric
    {
        VersionDescriptor a{"x","1.0.0-1",VersionScheme::semver};
        VersionDescriptor b{"x","1.0.0-alpha",VersionScheme::semver};
        CHECK(compareVersions(a,b) == Comparison::less, "numeric prerelease < alpha");
    }

    // Build metadata ignored in compare
    {
        VersionDescriptor a{"x","1.0.0+build1",VersionScheme::semver};
        VersionDescriptor b{"x","1.0.0+build2",VersionScheme::semver};
        CHECK(compareVersions(a,b) == Comparison::equal, "build ignored");
    }

    // Delimiter collision separation check
    {
        InstallationInstanceDescriptor d1{"npm","user","/root","stable","a:b",""};
        InstallationInstanceDescriptor d2{"npm","user","/root","stable","a","b:"};
        IdentityResult r1 = makeInstallationInstanceKey(d1);
        IdentityResult r2 = makeInstallationInstanceKey(d2);
        CHECK(r1.key != r2.key, "delimiter collision separation");
    }

    // Version omitted from identity: same key regardless of version concept (no version field exists) - trivially covered by struct

    // Empty prerelease/build suffix rejection
    {
        VersionDescriptor d{"x","1.2.3-",VersionScheme::semver};
        CHECK(parseVersion(d).status == ParseStatus::invalid, "empty prerelease suffix");
    }
    {
        VersionDescriptor d{"x","1.2.3+",VersionScheme::semver};
        CHECK(parseVersion(d).status == ParseStatus::invalid, "empty build suffix");
    }
    {
        VersionDescriptor d{"x","1.2.3-+build",VersionScheme::semver};
        CHECK(parseVersion(d).status == ParseStatus::invalid, "empty prerelease before build");
    }

    // ASCII-only identity normalization
    {
        // non-ASCII whitespace should not be trimmed (stays part of value)
        InstallationInstanceDescriptor d{"NPM","USER","/root\u00A0","stable","pkg",""};
        IdentityResult r = makeInstallationInstanceKey(d);
        CHECK(r.error == IdentityError::none, "non-ascii space not trimmed but valid");
    }

    // ASCII whitespace trimming on all identity fields
    {
        InstallationInstanceDescriptor dirty{"  NPM  ", "  USER  ", "  /root  ", "  stable  ", "  pkg  ", "  q  "};
        InstallationInstanceDescriptor clean{"npm", "user", "/root", "stable", "pkg", "q"};
        IdentityResult a = makeInstallationInstanceKey(dirty);
        IdentityResult b = makeInstallationInstanceKey(clean);
        CHECK(a.key == b.key, "ascii whitespace trimmed all identity fields");
    }
    // non-ASCII whitespace preserved in installRoot
    {
        InstallationInstanceDescriptor a{"npm", "user", "/root\xc2\xa0", "stable", "pkg", ""};
        InstallationInstanceDescriptor b{"npm", "user", "/root", "stable", "pkg", ""};
        IdentityResult ra = makeInstallationInstanceKey(a);
        IdentityResult rb = makeInstallationInstanceKey(b);
        CHECK(ra.key != rb.key, "non-ascii whitespace preserved in installRoot");
    }

    if (failures > 0) {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All tests passed.\n";
    return 0;
}