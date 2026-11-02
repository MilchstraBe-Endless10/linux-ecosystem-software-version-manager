# Flatpak CLI adapter (production source)

Non-deploying adapter that invokes Flatpak through `QProcess` and maps installed
refs and remote update candidates into the shared Core model. It never applies
application updates.

## Official contract

- Reference: https://docs.flatpak.org/en/latest/flatpak-command-reference.html
- Default query: `flatpak list --app --columns=...` (user + default system).
- Named system installations are enumerated from `/etc/flatpak/installations.d/*.conf` and queried explicitly with `--installation=NAME`.
- Installation roots and identities are preserved; duplicate identities are skipped.
- Enumeration or per-installation failures are partial errors; successful default results are retained.
- An absent `/etc/flatpak/installations.d` directory is optional and means zero named installations with no error; a present but unreadable, non-directory, or malformed configuration remains explicit partial/error evidence. Entries are enumerated without silently omitting unreadable `.conf` files, and a valid sibling name remains available when another entry is unreadable or invalid.
- Malformed query output fails closed.
- `--app` restricts output to applications (omits runtimes).
- `--columns` supports `application`, `arch`, `branch`, `version`, `origin`,
  `installation` (among others). Field names may be abbreviated to a unique
  prefix.

## Command

```
flatpak list --app --columns=application,arch,branch,version,origin,installation,active
flatpak remote-ls --updates --app \
  --columns=application,arch,branch,version,commit
```

The CLI output format is **not** documented as a stable machine protocol. In
practice, `--columns` output written to a pipe is headerless, tab-delimited TSV
(https://github.com/flatpak/flatpak/issues/4720). The parser accepts this real
CLI form and headered fixtures, and fails closed on non-TSV human tables,
ambiguous, truncated, or malformed records. Successful zero-byte output is a
valid empty result; a nonzero cache-miss exit remains an unavailable source,
not an empty scan. An empty `Version` field is preserved as unknown/empty opaque
metadata because AppStream may omit it.

Flatpak 1.18.4 abbreviated both `active` and `commit` to 12 hexadecimal
characters even when the documented `:full` column modifier was requested.
The adapter therefore resolves a 12-character installed prefix with
`flatpak info --show-commit`, and a 12-character update prefix with
`flatpak remote-info --show-commit`, each pinned to architecture, branch,
remote, and installation. Candidate evidence stores the full 64-digit OSTree
commit. The candidate check uses ordinary `remote-ls --updates` without
`--cached`. This is not a zero-write query: Flatpak 1.18.4 fetches remote
summary/index metadata into its cache directory and can prune older summary
cache entries. These are local metadata-cache writes; the adapter does not
deploy an application update or change an installed commit. An isolated Flatpak
1.18.4 run reproduced `--cached` failing with
“No cached summary” when a newly configured remote had no local summary; the
ordinary query succeeded against the isolated test remote. The adapter queries
remote metadata but never deploys an application update. The CLI does not
provide a source timestamp, so freshness remains unknown even when the query
succeeds. Query failures remain `unavailable`, never an inferred match or
update.

## Identity

Each row preserves app ID, scope (user/system derived from the installation
column), the absolute installation data root, architecture, branch, and origin.
The user root resolves through `FLATPAK_USER_DIR`, then `XDG_DATA_HOME/flatpak`,
then `$HOME/.local/share/flatpak`. The default system root resolves through
`FLATPAK_SYSTEM_DIR`, then `/var/lib/flatpak`. Named system installations are
resolved from their exact `[Installation "NAME"]` `Path=` entry in a readable
`.conf` file under `/etc/flatpak/installations.d/`; an unknown or non-absolute root
causes that row to be skipped and the snapshot marked partial. Flatpak documents
the user/system locations and named installation configuration in its
[command reference](https://docs.flatpak.org/en/latest/flatpak-command-reference.html).
Versions are stored as opaque descriptors and are never compared or ordered.
Update candidates are identified by a different OSTree commit, not by version
string comparison. Flatpak documents `remote-ls --cached` as preferring local
information even when it may be out of date. The adapter uses the ordinary
remote query so first checks do not depend on a pre-existing local summary
cache. This can populate Flatpak's local summary/index cache, but does not
deploy an app. Therefore an empty remote update list is recorded as
`no_candidate`, not `up_to_date`; an exact match to the
installed commit establishes `observed_revision_matches`. A Flatpak result is
`up_to_date` only when the source check has explicit fresh evidence. The CLI
does not report summary publication time, so source freshness remains unknown.
The reported version is AppStream display metadata and may be absent or stale; the
candidate commit is the update identity evidence. Evidence retains the exact
`app/<id>/<arch>/<branch>` ref, installation scope/root, remote and query time.
Core rejects evidence that crosses any of those identity boundaries. Freshness
remains independent, so an equal observed commit does not prove that the source is
currently latest.

## Process execution

`QProcess` runs with a strict timeout and an output cap. Unavailable CLI,
timeout, truncation, and nonzero exit are reported as explicit partial states
with `SourceError` entries. Raw stderr is omitted from those errors because it
may contain repository URLs or credentials. Process execution and root
resolution are injectable (`ProcessRunner`, `InstallRootResolver`) so tests use
synthetic results only; no `flatpak` command is run on the host during tests.

## Tests

`tests.cpp` contains fixture-based parser and error tests using synthetic
output and injected runner results only. `tests/flatpak_cli_real_integration.py`
and its C++ driver create temporary user/system installations and a local test
remote, then run the real Flatpak 1.18.4 CLI under bubblewrap. The integration
asserts that first-query summary-cache writes stay within Flatpak's cache
directories, that installed commits do not change, and that unavailable remote
data remains unavailable through Core and Qt. No host scan or real package
update is performed.

The GitHub Actions workflow `.github/workflows/flatpak-cli-integration.yml` is
configured for the public repository's current source baseline. It uses a
focused CMake entry point at `tests/flatpak-cli-ci/` to build the Flatpak
adapter, Core model, Qt model, and integration driver without depending on the
newer local GUI tree. On Ubuntu 24.04 it adds the `ppa:flatpak/stable` source,
whose Launchpad signing-key fingerprint is
`5C6D153A17C02C337EF6C663B8B9D41229DFA5F5`, logs `apt-cache policy flatpak`
to retain the selected repository and version in the Actions log, installs the exact package
`1.18.4-1~flatpak1~24.04.1`, and asserts `flatpak --version` is `Flatpak 1.18.4`
before configuring or running tests. Qt 6.8's Linux desktop architecture is
`linux_gcc_64` (Qt 6.7 and later naming), not the older `gcc_64` identifier.
It then runs the focused Flatpak/Core/Qt
CTest suite and the real CLI fixture in a bubblewrap network namespace with
loopback as its only network interface. The first hosted workflow run is still
required to validate the CI environment itself.
