# INVENTORY-001 · Real installed software and original desktop icons

**Stage:** read-only metadata discovery prototype, not the complete K3 adapter layer.

## What runs today

`inventory.py` reads the local **pacman database** (`/var/lib/pacman/local/*/desc`) and XDG `applications/*.desktop` entries from standard user/system/Flatpak export locations. The resulting JSON has actual detected package/launcher counts, installed pacman versions, desktop `Icon=` theme names, and sources/provenance. If the pacman `files` manifest contains a matching desktop launcher, the link is recorded as pacman ownership **without guessing by app name**.

```bash
python3 experiments/inventory/read-only-001/inventory.py > /tmp/linux-inventory-local.json
python3 -m unittest discover -s experiments/inventory/read-only-001 -p 'test_*.py' -v
```

This is read-only: **no subprocess, sudo, package-manager operation, remote update check, launcher execution, exhaustive filesystem scan or credential access**. It never invents a newest version or update badge: `latest_version=null`, `update_status=unknown` until a trustworthy update adapter supplies evidence.

Synthetic fixtures in the tests are **only for tests**. The CLI's default roots are genuine local metadata directories. A missing/unreadable source is reported as a warning, not as an empty healthy system.

## Icon rules (essential)

1. Linux GUI launchers typically declare `Icon=...` in a Freedesktop `.desktop` entry; a **theme name** should be resolved by Qt `QIcon::fromTheme()`, preserving the user's icon theme. No arbitrary single-letter emoji or Unicode symbol for a known application.
2. Some entries contain **absolute icon paths**. This prototype deliberately returns `absolute-path-requires-verification` until the host Qt/GIO layer can validate the file is an approved icon resource. Do not blindly load arbitrary paths supplied by desktop files.
3. For package metadata without desktop entries, source-backed **AppStream icons** may be used with provenance and license review. Packages, runtimes and CLI tools sometimes have **no registered application icon**; show a clearly generic *category* placeholder or verified official brand asset, never pretend a guessed glyph is the official logo.
4. Bind icons in GUI using a native `QQuickImageProvider` backed by `QIcon::fromTheme` / `GIcon`. Cache **size-limited** 32px/64px variants, not raw 512px textures for every row. Package-to-app associations must be derived from trusted ownership/source IDs, not fuzzy names.
5. Desktop entry `Hidden=true` at higher XDG priority suppresses the lower-priority entry. `NoDisplay=true` is retained as metadata rather than silently hiding the installed software from a system manager.

Sources: [Freedesktop Desktop Entry spec](https://specifications.freedesktop.org/desktop-entry/latest-single/), [Qt QIcon](https://doc.qt.io/qt-6/qicon.html), [GDesktopAppInfo](https://gnome.pages.gitlab.gnome.org/gtk/gio-unix/class.DesktopAppInfo.html).

## Scope limits and next modules

| Manager/source | This prototype | Production-ready direction |
| --- | --- | --- |
| pacman/Arch | Installed names and versions from local DB | **libalpm** API for efficient, verified local DB and ownership |
| Desktop GUI launchers | Name, themed icon token, provenance | **GIO GDesktopAppInfo/GIcon + Qt image provider** |
| Flatpak | Exported desktop launchers only | **libflatpak** installed refs + user/system installations, remote evidence |
| npm/pnpm/Bun | Not discovered | Read-only manager-specific listing/adapters |
| pipx/uv tool | Not discovered | Read-only tool lists and install roots |
| mise/Cargo/rustup/other CLI | Not discovered | Separate source adapters, no full-disk scan |

Production UI **must not** show any fake `10000`, `286`, `latest`, `3 instances`, health, or risk value as real. A 10k/100k synthetic model belongs only behind explicit `--stress-test` or test fixtures and must be marked `MOCK` in the interface.

**This is not yet wired to the user's current private GUI-POC-005/Qt model.** Codex should port this parser/provenance contract to a thread-safe native data model, keeping the current GUI and private assets intact. DSH can add independently bounded manager adapters under Codex supervision. No unattended DSH writes until SEC-001 is resolved.

## Performance guardrails

- No blocking package/desktop scans on the QML GUI thread. Perform capped read-only discovery in a worker and batch deltas into `QAbstractListModel` / `QAbstractTableModel`.
- Use stable instance IDs and incremental `dataChanged`/insert/remove instead of full model resets on each refresh. Virtualize delegates (`ListView.reuseItems`), avoid per-row shader blur.
- Cache icon pixmaps by **theme name + icon identity + requested px + DPR**, with a memory ceiling and invalidation on icon-theme changes. Avoid retaining 10,000 full-resolution textures.
- Lazy-load details, downsample to on-screen size, pause particles/shaders while minimized or reduced-motion, and dynamically reduce effects on low-power GPUs.
- Measure startup, scan latency, Qt GUI-thread stalls, P95/P99 frame-time, CPU, RSS, texture memory and actual Wayland dimensions on the target machine. **No performance SLA has been demonstrated by this prototype.**

## Testing and safety

The offline 10-test suite covers installed version truth, icon name/source, ownership, symlink handling, Hidden overrides, invalid icon paths, malformed files and missing sources. The code does not execute `Exec=`. The public branch includes **only test-made synthetic fixtures, not any real host inventory or private icon files**.

**Next acceptance:** Codex integrates real counts + real icons into interactive QML, replaces the default fake list with an actual local scan (only on explicit controlled startup), retains stress mode separately, and confirms it never marks unknown as latest. SEC-001 stays a separate security track.
