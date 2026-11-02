#include "version_id.hpp"

#include <algorithm>
#include <string>

namespace version_id_001 {

namespace {

bool isAsciiSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::string trimAscii(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && isAsciiSpace(s[start])) start++;
    size_t end = s.size();
    while (end > start && isAsciiSpace(s[end - 1])) end--;
    return s.substr(start, end - start);
}

bool isAsciiDigit(char c) { return c >= '0' && c <= '9'; }

bool isAsciiAlpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isAsciiAlnum(char c) { return isAsciiDigit(c) || isAsciiAlpha(c); }

bool allDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!isAsciiDigit(c)) return false;
    return true;
}

char asciiToLower(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c + 32);
    return c;
}

int cmpNumericStr(const std::string& a, const std::string& b) {
    std::string aa = a, bb = b;
    while (aa.size() > 1 && aa[0] == '0') aa.erase(aa.begin());
    while (bb.size() > 1 && bb[0] == '0') bb.erase(bb.begin());
    if (aa.size() != bb.size()) return aa.size() < bb.size() ? -1 : 1;
    if (aa == bb) return 0;
    return aa < bb ? -1 : 1;
}

bool parseSemVer(const std::string& raw, SemanticVersion& out) {
    std::string s = raw;
    // reject surrounding whitespace (ASCII)
    if (s != trimAscii(s)) return false;
    if (s.empty()) return false;
    // reject leading v
    if (s[0] == 'v' || s[0] == 'V') return false;

    bool hasBuild = false;
    bool hasPre = false;
    std::string buildPart;
    size_t plus = s.find('+');
    if (plus != std::string::npos) {
        hasBuild = true;
        buildPart = s.substr(plus + 1);
        s = s.substr(0, plus);
    }
    std::string prePart;
    size_t dash = s.find('-');
    if (dash != std::string::npos) {
        hasPre = true;
        prePart = s.substr(dash + 1);
        s = s.substr(0, dash);
    }

    // reject empty suffixes
    if (hasPre && prePart.empty()) return false;
    if (hasBuild && buildPart.empty()) return false;

    // Core
    std::vector<std::string> core;
    {
        std::string cur;
        for (char c : s) {
            if (c == '.') { core.push_back(cur); cur.clear(); }
            else cur += c;
        }
        core.push_back(cur);
    }
    if (core.size() != 3) return false;
    for (auto& c : core) {
        if (!allDigits(c)) return false;
        if (c.size() > 1 && c[0] == '0') return false;
    }
    out.major = core[0];
    out.minor = core[1];
    out.patch = core[2];

    out.prerelease.clear();
    if (hasPre) {
        std::string cur;
        for (char c : prePart) {
            if (c == '.') {
                if (cur.empty()) return false;
                out.prerelease.push_back(cur);
                cur.clear();
            } else cur += c;
        }
        if (cur.empty()) return false;
        out.prerelease.push_back(cur);
        for (auto& id : out.prerelease) {
            if (id.empty()) return false;
            if (allDigits(id)) {
                if (id.size() > 1 && id[0] == '0') return false;
            } else {
                for (char c : id) {
                    if (!(isAsciiAlnum(c) || c == '-')) return false;
                }
            }
        }
    }

    out.build.clear();
    if (hasBuild) {
        std::string cur;
        for (char c : buildPart) {
            if (c == '.') {
                if (cur.empty()) return false;
                out.build.push_back(cur);
                cur.clear();
            } else cur += c;
        }
        if (cur.empty()) return false;
        out.build.push_back(cur);
        for (auto& id : out.build) {
            if (id.empty()) return false;
            for (char c : id) {
                if (!(isAsciiAlnum(c) || c == '-')) return false;
            }
        }
    }

    return true;
}

int cmpPrerelease(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    if (a.empty() && b.empty()) return 0;
    if (a.empty()) return 1;
    if (b.empty()) return -1;
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        const std::string& x = a[i];
        const std::string& y = b[i];
        bool xn = allDigits(x), yn = allDigits(y);
        if (xn && yn) {
            int c = cmpNumericStr(x, y);
            if (c != 0) return c;
        } else if (xn != yn) {
            return xn ? -1 : 1;
        } else {
            if (x != y) return x < y ? -1 : 1;
        }
    }
    if (a.size() == b.size()) return 0;
    return a.size() < b.size() ? -1 : 1;
}

} // anonymous

ParseResult parseVersion(const VersionDescriptor& descriptor) {
    ParseResult result;
    result.status = ParseStatus::invalid;
    if (descriptor.scheme != VersionScheme::semver) {
        result.status = ParseStatus::unsupported;
        return result;
    }
    SemanticVersion sv;
    if (parseSemVer(descriptor.raw, sv)) {
        result.status = ParseStatus::valid;
        result.version = sv;
    } else {
        result.status = ParseStatus::invalid;
    }
    return result;
}

Comparison compareVersions(const VersionDescriptor& left, const VersionDescriptor& right) {
    if (left.scheme != VersionScheme::semver || right.scheme != VersionScheme::semver) {
        return Comparison::incomparable;
    }
    ParseResult pl = parseVersion(left);
    ParseResult pr = parseVersion(right);
    if (pl.status != ParseStatus::valid || pr.status != ParseStatus::valid) {
        return Comparison::incomparable;
    }
    const SemanticVersion& a = pl.version;
    const SemanticVersion& b = pr.version;
    int c;
    c = cmpNumericStr(a.major, b.major);
    if (c != 0) return c < 0 ? Comparison::less : Comparison::greater;
    c = cmpNumericStr(a.minor, b.minor);
    if (c != 0) return c < 0 ? Comparison::less : Comparison::greater;
    c = cmpNumericStr(a.patch, b.patch);
    if (c != 0) return c < 0 ? Comparison::less : Comparison::greater;
    c = cmpPrerelease(a.prerelease, b.prerelease);
    if (c != 0) return c < 0 ? Comparison::less : Comparison::greater;
    return Comparison::equal;
}

IdentityResult makeInstallationInstanceKey(const InstallationInstanceDescriptor& descriptor) {
    IdentityResult result;
    std::string manager = trimAscii(descriptor.manager);
    std::string scope = trimAscii(descriptor.scope);
    std::string installRoot = trimAscii(descriptor.installRoot);
    std::string channel = trimAscii(descriptor.channel);
    std::string packageId = trimAscii(descriptor.packageId);
    std::string qualifier = trimAscii(descriptor.instanceQualifier);

    std::string mLow, sLow;
    for (char c : manager) mLow += asciiToLower(c);
    for (char c : scope) sLow += asciiToLower(c);

    if (mLow.empty()) { result.error = IdentityError::missingManager; return result; }
    if (sLow.empty()) { result.error = IdentityError::missingScope; return result; }
    if (installRoot.empty()) { result.error = IdentityError::missingInstallRoot; return result; }
    if (packageId.empty()) { result.error = IdentityError::missingPackageId; return result; }

    std::string key = "vi1:";
    auto appendField = [&key](const std::string& val) {
        key += std::to_string(val.size());
        key += ":";
        key += val;
        key += ";";
    };
    appendField(mLow);
    appendField(sLow);
    appendField(installRoot);
    appendField(channel);
    appendField(packageId);
    appendField(qualifier);

    result.key = key;
    result.error = IdentityError::none;
    return result;
}

} // namespace version_id_001