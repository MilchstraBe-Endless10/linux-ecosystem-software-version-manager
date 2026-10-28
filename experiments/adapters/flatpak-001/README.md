# FLATPAK-ADAPTER-001: mock-only source normalization

A standalone C++17/CTest implementation for the Linux ecosystem update manager. It **does not launch Flatpak**, read the local filesystem, access the network, check for updates, or install/update anything. The caller supplies already captured, reviewed synthetic TSV strings.

Installed and update-candidate input: exactly five tab-separated columns `application, arch, branch, version, origin`. These correspond to the official Flatpak `--columns` output for `flatpak list --app` and `flatpak remote-ls --updates --app` (this module never executes those commands). Flatpak `remote-ls` lacks an installation column. The caller therefore passes the origin installation context separately for each snapshot (`user`, `system` or `installation:NAME`).

Identity uses installation scope + application ID + arch + branch + origin. Length-prefixing prevents composite-key dimension collisions. The reconciler marks a Candidate **only when present in an explicit updates snapshot**, never by naïve SemVer comparison. Versions may be equal or blank (e.g. a changed Flatpak commit with unchanged display version). A missing candidate is Unknown, not guaranteed latest. Duplicated or unmatched records are reported as issues.

The fixture parser is intentionally strict: the caller decides whether a localized header is present; tab-separated rows must contain exactly five fields, with nonempty identity fields. It does not promise robust parsing of arbitrary terminal output. Live command semantics, error handling, installation enumeration, and Flatpak version variations require additional Codex acceptance before real scanning.

## Build and test

Requirements: CMake >= 3.16, C++17 compiler; no Qt or Flatpak dependency.

    cmake -S experiments/adapters/flatpak-001 -B /tmp/flatpak-001-build -DCMAKE_BUILD_TYPE=Release
    cmake --build /tmp/flatpak-001-build --parallel
    ctest --test-dir /tmp/flatpak-001-build --output-on-failure

## Status

Public development experiment, tracked by [Issue #5](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/5). Codex review, target Omarchy build, integration with the independently developed VERSION-ID-001 and release licensing are pending. No project-wide code license has been selected, and this branch is not production-ready.
