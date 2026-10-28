#include "flatpak_snapshot.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using ecosystem::flatpak::ParseResult;
using ecosystem::flatpak::Row;
using ecosystem::flatpak::UpdateStatus;
using ecosystem::flatpak::identity_key;
using ecosystem::flatpak::parse_tsv;
using ecosystem::flatpak::reconcile;

namespace {
int assertions = 0;
void check(bool ok, const char* label) {
    ++assertions;
    if (!ok) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}

constexpr const char* header = "App\tArch\tBranch\tVersion\tOrigin\n";

std::string one(const char* version, const char* origin = "flathub") {
    return std::string(header) + "org.example.App\tx86_64\tstable\t" + version
         + "\t" + origin + "\n";
}
}

int main() {
    const auto user = parse_tsv(one("1.0"), "user");
    check(user.rows.size() == 1 && user.issues.empty(), "parse one row");
    check(user.rows[0].version == "1.0", "preserve current version");
    const auto system = parse_tsv(one("1.0"), "system");
    check(identity_key(user.rows[0]) != identity_key(system.rows[0]), "scope isolates instances");
    const auto alternate = parse_tsv(one("1.0", "private"), "user");
    check(identity_key(user.rows[0]) != identity_key(alternate.rows[0]), "origin isolates instances");
    Row branch = user.rows[0]; branch.branch = "beta";
    check(identity_key(user.rows[0]) != identity_key(branch), "branch isolates instances");
    Row arch = user.rows[0]; arch.arch = "aarch64";
    check(identity_key(user.rows[0]) != identity_key(arch), "arch isolates instances");
    check(parse_tsv(one("1.0"), "installation:games").rows.size() == 1,
          "named installations accepted");
    check(!parse_tsv(one("1.0"), "installation:../secrets").issues.empty(),
          "reject unsafe installation name");
    check(!parse_tsv(one("1.0"), "bad").issues.empty(), "reject unknown scope");
    auto crlf = parse_tsv("App\tArch\tBranch\tVersion\tOrigin\r\norg.example.App\tx86_64\tstable\t1.0\tflathub\r\n", "user");
    check(crlf.rows.size() == 1 && crlf.issues.empty(), "CRLF supported");
    auto localized = parse_tsv("Anwendung\tArchitektur\tZweig\tVersion\tQuelle\norg.example.App\tx86_64\tstable\t1.0\tflathub\n", "user");
    check(localized.rows.size() == 1 && localized.issues.empty(), "localized header skipped by position");
    check(parse_tsv("org.example.App\tx86_64\tstable\t1.0\tflathub", "user", false).rows.size() == 1,
          "headerless input");
    check(!parse_tsv(std::string(header) + "org.example.App\tx86_64\tstable\n", "user").issues.empty(),
          "malformed row rejected");
    check(!parse_tsv(std::string(header) + "\tx86_64\tstable\t1.0\tflathub\n", "user").issues.empty(),
          "missing identity rejected");
    check(!parse_tsv(std::string(header) + "org.example.App\tx86_64\tstable\t\x1b[31m\tflathub\n", "user").issues.empty(),
          "escape sequence rejected");
    auto empty_version = parse_tsv(one(""), "user");
    check(empty_version.rows.size() == 1, "unknown version retained");
    auto updates = parse_tsv(one("2.0"), "user");
    auto resolved = reconcile(user, updates);
    check(resolved.rows.size() == 1 && resolved.issues.empty(), "candidate joined");
    check(resolved.rows[0].status == UpdateStatus::Candidate && resolved.rows[0].candidate_version == "2.0", "explicit candidate status");
    auto same_version = reconcile(user, parse_tsv(one("1.0"), "user"));
    check(same_version.rows[0].status == UpdateStatus::Candidate,
          "do not infer from version equality");
    auto missing = reconcile(user, parse_tsv(header, "user"));
    check(missing.rows[0].status == UpdateStatus::Unknown, "missing candidate remains unknown");
    auto wrong_scope = reconcile(user, parse_tsv(one("2.0"), "system"));
    check(wrong_scope.rows[0].status == UpdateStatus::Unknown && !wrong_scope.issues.empty(),
          "no cross-installation joins");
    auto wrong_origin = reconcile(user, parse_tsv(one("2.0", "private"), "user"));
    check(wrong_origin.rows[0].status == UpdateStatus::Unknown && !wrong_origin.issues.empty(),
          "no cross-origin joins");
    auto duplicate_installed = reconcile(parse_tsv(one("1.0") + "org.example.App\tx86_64\tstable\t1.0\tflathub\n", "user"), updates);
    check(!duplicate_installed.issues.empty() && duplicate_installed.rows.size() == 1,
          "duplicate installed identity flagged");
    auto duplicate_updates = reconcile(user, parse_tsv(one("2.0") + "org.example.App\tx86_64\tstable\t2.1\tflathub\n", "user"));
    check(!duplicate_updates.issues.empty(), "duplicate candidates flagged");
    Row x = user.rows[0]; Row y = x;
    x.application = "ab"; x.branch = "c";
    y.application = "a"; y.branch = "bc";
    check(identity_key(x) != identity_key(y), "identity dimension collision prevention");
    std::cout << "OK " << assertions << " assertions\n";
}
