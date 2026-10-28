#!/usr/bin/env python3
"""Bounded read-only pacman + XDG desktop discovery for Linux.

Prototype only: no subprocess, network, package updates, desktop launching, or
claims of Flatpak/npm/pipx/uv/mise coverage. Output remains unknown for updates.
"""
from __future__ import annotations
import argparse
import configparser
import json
import os
from pathlib import Path
import re

MAX_ENTRY_BYTES = 512_000
MAX_PER_ROOT = 60_000
ICON_NAME = re.compile(r"^[\w.\-+]+$", re.UNICODE)


def _read_small(path: Path) -> str:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_ENTRY_BYTES:
        raise ValueError("not an eligible small regular metadata file")
    return path.read_text(encoding="utf-8", errors="replace")


def _pacman_sections(text: str) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    current = None
    for line in text.splitlines():
        if line.startswith("%") and line.endswith("%") and len(line) > 2:
            current = line[1:-1]
            result.setdefault(current, [])
        elif current is not None and line:
            result[current].append(line)
    return result


def scan_pacman(root: Path) -> tuple[list[dict], dict[str, str], list[str]]:
    packages, owners, warnings = [], {}, []
    if not root.is_dir() or root.is_symlink():
        return packages, owners, ["pacman database unavailable"]
    try:
        children = sorted(root.iterdir())
    except OSError:
        return packages, owners, ["pacman database could not be enumerated"]
    for i, folder in enumerate(children):
        if i >= MAX_PER_ROOT:
            warnings.append("pacman scan limit reached")
            break
        if folder.is_symlink() or not folder.is_dir():
            continue
        try:
            fields = _pacman_sections(_read_small(folder / "desc"))
            name = (fields.get("NAME") or [""])[0]
            version = (fields.get("VERSION") or [""])[0]
            if not name or not version:
                warnings.append("skipped malformed pacman metadata")
                continue
            packages.append({"manager": "pacman", "package_id": name,
                             "display_name": name, "installed_version": version,
                             "latest_version": None, "update_status": "unknown",
                             "icon_name": None, "icon_path": None,
                             "icon_provenance": "missing",
                             "source_evidence": "pacman-local-db"})
            try:
                paths = _pacman_sections(_read_small(folder / "files")).get("FILES", [])
            except (OSError, ValueError):
                paths = []
            for rel in paths:
                if rel.startswith("usr/share/applications/") and rel.endswith(".desktop"):
                    owners[rel.removeprefix("usr/share/applications/")] = name
        except (OSError, ValueError):
            warnings.append("unreadable pacman package metadata")
    return packages, owners, warnings


def _icon_info(raw: str) -> tuple[str | None, str | None, str]:
    raw = raw.strip()
    if not raw or len(raw) > 512:
        return None, None, "missing"
    if raw.startswith("/"):
        # Do not automatically display arbitrary filesystem paths from desktop metadata.
        return None, None, "absolute-path-requires-verification"
    if ICON_NAME.fullmatch(raw) and raw not in (".", ".."):
        return raw, None, "desktop-entry-theme"
    return None, None, "invalid-desktop-icon"


def _read_desktop(path: Path) -> dict | None:
    ini = configparser.ConfigParser(interpolation=None, strict=False, delimiters=("=",))
    ini.optionxform = str
    ini.read_string(_read_small(path))
    if not ini.has_section("Desktop Entry"):
        return None
    x = ini["Desktop Entry"]
    if x.get("Type", "Application") != "Application":
        return None
    if x.get("Hidden", "false").lower() == "true":
        return {"_hidden": True}
    name = x.get("Name", "").strip()
    if not name:
        return None
    icon_name, icon_path, provenance = _icon_info(x.get("Icon", ""))
    return {"manager": "xdg", "desktop_id": path.name, "display_name": name,
            "icon_name": icon_name, "icon_path": icon_path,
            "icon_provenance": provenance, "source_evidence": "xdg-desktop-entry",
            "no_display": x.get("NoDisplay", "false").lower() == "true",
            "installed_version": None, "latest_version": None,
            "update_status": "unknown", "package_id": None}


def scan_desktop(roots: list[tuple[Path, str]], package_owners: dict[str, str]) -> tuple[list[dict], list[str]]:
    apps, warnings, seen = [], [], set()
    for root, manager in roots:
        directory = root / "applications"
        if not directory.is_dir() or directory.is_symlink():
            continue
        try:
            candidates = sorted(directory.glob("*.desktop"))
        except OSError:
            warnings.append("desktop directory could not be enumerated")
            continue
        for i, path in enumerate(candidates):
            if i >= MAX_PER_ROOT:
                warnings.append("desktop entry scan limit reached")
                break
            if path.name in seen:
                continue
            try:
                app = _read_desktop(path)
                if app is None:
                    continue
                seen.add(path.name)
                if app.get("_hidden"):
                    continue  # User's Hidden=true masks lower-priority system launcher.
                app["manager"] = manager
                if manager == "xdg-system" and path.name in package_owners:
                    app["package_id"] = package_owners[path.name]
                    app["manager"] = "pacman"
                    app["source_evidence"] = "pacman-files+xdg-desktop-entry"
                apps.append(app)
            except (OSError, ValueError, configparser.Error):
                warnings.append("skipped unreadable or malformed desktop entry")
    return apps, warnings


def default_roots() -> list[tuple[Path, str]]:
    home = Path.home()
    xdg_home = Path(os.environ.get("XDG_DATA_HOME", str(home / ".local/share")))
    system_dirs = os.environ.get("XDG_DATA_DIRS", "/usr/local/share:/usr/share").split(":")
    roots = [(xdg_home, "xdg-user"),
             (home / ".local/share/flatpak/exports/share", "flatpak-user")]
    roots.extend((Path(d), "xdg-system") for d in system_dirs if d)
    roots.append((Path("/var/lib/flatpak/exports/share"), "flatpak-system"))
    return roots


def discover(pacman_db: Path, roots: list[tuple[Path, str]]) -> dict:
    packages, owners, w1 = scan_pacman(pacman_db)
    launchers, w2 = scan_desktop(roots, owners)
    return {"schema_version": "0.1", "mode": "read-only-local-metadata",
            "coverage": ["pacman-local-db", "xdg-desktop-entries"],
            "packages": packages, "launchers": launchers,
            "counts": {"pacman_packages": len(packages),
                       "desktop_launchers": len(launchers)},
            "warnings": w1 + w2,
            "note": "Discovered counts, not synthetic. Update state unknown. Other managers need separate adapters."}


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pacman-db", type=Path, default=Path("/var/lib/pacman/local"))
    p.add_argument("--desktop-data-root", type=Path, action="append",
                   help="XDG share dir containing applications/, overrides normal roots")
    args = p.parse_args()
    roots = [(r, "xdg-system") for r in args.desktop_data_root] if args.desktop_data_root else default_roots()
    print(json.dumps(discover(args.pacman_db, roots), ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
