"""Synthetic single-run process observer fixture. Not an OS sandbox."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys
import time

EXPECTED = b"sec-001b1-synthetic-v1"


def verify_fixture(contents: bytes) -> None:
    """Allow one optional LF or CRLF, avoiding double-newline assumptions."""
    if contents not in (EXPECTED, EXPECTED + b"\n", EXPECTED + b"\r\n"):
        raise ValueError("synthetic fixture mismatch")


def ns_ids() -> dict[str, str]:
    result = {}
    for name in ("user", "mnt", "pid", "ipc", "uts", "net"):
        try:
            result[name] = os.readlink(f"/proc/self/ns/{name}")
        except OSError:
            result[name] = "unavailable"
    return result


def run(workdir: Path, hold_seconds: int) -> None:
    if hold_seconds < 0 or hold_seconds > 30:
        raise ValueError("hold_seconds must be in [0, 30]")
    # Only synthetic data. Neither these checks nor this process enforce
    # operating-system-level sandbox boundaries.
    if not workdir.is_dir() or workdir.is_symlink():
        raise ValueError("synthetic workdir must be a real directory")
    fixture = workdir / "fixture.txt"
    if fixture.is_symlink():
        raise ValueError("fixture must not be a symlink")
    verify_fixture(fixture.read_bytes())
    result_path = workdir / "result.json"
    if result_path.exists() or result_path.is_symlink():
        raise ValueError("result already exists; do not overwrite or re-run")
    evidence = {
        "fixture": "validated",
        "pid": os.getpid(),
        "ppid": os.getppid(),
        "uid": os.getuid(),
        "gid": os.getgid(),
        "namespaces": ns_ids(),
        "phase": "ready",
    }
    # Private local evidence: never publish pid or namespace details to GitHub.
    with result_path.open("x", encoding="utf-8") as file:
        json.dump(evidence, file, sort_keys=True)
        file.write("\n")
    print("SEC001B1_READY", flush=True)
    time.sleep(hold_seconds)
    evidence["phase"] = "completed"
    temp_path = workdir / "result.complete.tmp"
    if temp_path.exists() or temp_path.is_symlink():
        raise ValueError("stale temporary result")
    with temp_path.open("x", encoding="utf-8") as file:
        json.dump(evidence, file, sort_keys=True)
        file.write("\n")
    temp_path.replace(result_path)
    print("SEC001B1_DONE", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workdir", type=Path, required=True)
    parser.add_argument("--hold-seconds", type=int, default=12)
    args = parser.parse_args()
    try:
        run(args.workdir, args.hold_seconds)
        return 0
    except (OSError, ValueError) as exc:
        print(f"SEC001B1_FAILED: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
