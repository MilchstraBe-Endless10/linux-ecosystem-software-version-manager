#!/usr/bin/env python3
"""Run Flatpak 1.18.4 against throwaway local user/system installations."""

from __future__ import annotations

import argparse
import hashlib
import http.server
import os
from pathlib import Path
import shutil
import socketserver
import subprocess
import tempfile
import threading
import time


REMOTE = "ci-synthetic"
APP_ID = "org.test.Hello"
RUNTIME_ID = "org.test.Platform"


def run(argv: list[str], env: dict[str, str], *, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(argv, env=env, text=True, capture_output=True, check=False)
    if expect_success and result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {argv!r}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, _format: str, *_args: object) -> None:
        pass


def start_local_repo_server(root: Path) -> tuple[socketserver.ThreadingMixIn, threading.Thread, str]:
    class Handler(QuietHandler):
        def __init__(self, *args: object, **kwargs: object) -> None:
            super().__init__(*args, directory=str(root), **kwargs)

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    host, port = server.server_address
    return server, thread, f"http://{host}:{port}/repo/"


def make_runtime(root: Path, env: dict[str, str], repo: Path) -> None:
    runtime = root / "runtime-build"
    (runtime / "files").mkdir(parents=True)
    (runtime / "usr" / "bin").mkdir(parents=True)
    (runtime / "var").mkdir()
    # Match Flatpak's own synthetic runtime fixture metadata: a base runtime
    # identifies itself by name and does not declare itself as its own SDK/runtime.
    (runtime / "metadata").write_text(
        f"[Runtime]\nname={RUNTIME_ID}\n",
        encoding="utf-8",
    )
    run(["flatpak", "build-export", "--no-update-summary", "--runtime",
         str(repo), str(runtime), "stable"], env)


def make_app(root: Path, env: dict[str, str], repo: Path, branch: str,
             payload: str, *, reexport: bool = False) -> Path:
    build = root / f"app-build-{branch}"
    if not reexport:
        # build-init requires an SDK already installed in a Flatpak installation.
        # This fixture tests repository install/update behavior, not SDK setup, so
        # create the minimal build metadata directly and keep all Flatpak export,
        # install, discovery, and update-query operations on the real CLI.
        build.mkdir(parents=True)
        (build / "files" / "bin").mkdir(parents=True)
        (build / "var").mkdir()
        arch = run(["flatpak", "--default-arch"], env).stdout.strip()
        (build / "metadata").write_text(
            "[Application]\n"
            f"name={APP_ID}\n"
            f"runtime={RUNTIME_ID}/{arch}/stable\n"
            f"sdk={RUNTIME_ID}/{arch}/stable\n",
            encoding="utf-8",
        )
    executable = build / "files" / "bin" / "hello.sh"
    executable.write_text(f"#!/bin/sh\nprintf '%s\\n' '{payload}'\n", encoding="utf-8")
    executable.chmod(0o755)
    if not reexport:
        run(["flatpak", "build-finish", "--command=hello.sh", str(build)], env)
    run(["flatpak", "build-export", "--disable-sandbox", "--no-update-summary",
         str(repo), str(build), branch], env)
    return build

def tree_files(root: Path) -> dict[str, tuple[str, str]]:
    result: dict[str, tuple[str, str]] = {}
    if not root.exists():
        return result
    for path in root.rglob("*"):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            result[relative] = ("symlink", os.readlink(path))
        elif path.is_file():
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            result[relative] = ("file", digest)
    return result


SUMMARY_CACHE_PATHS = (
    "cache/flatpak/system-cache/summaries",
    "system-cache/summaries",
    "user-flatpak/repo/summaries",
    "system-flatpak/repo/summaries",
    "user-flatpak/repo/tmp/cache/summaries",
    "system-flatpak/repo/tmp/cache/summaries",
)


def clear_summary_caches(root: Path) -> None:
    for relative in SUMMARY_CACHE_PATHS:
        shutil.rmtree(root / relative, ignore_errors=True)


def is_summary_cache_path(relative: str) -> bool:
    return any(relative.startswith(prefix + "/") or relative == prefix
               for prefix in SUMMARY_CACHE_PATHS)


def check_only_cache_writes(before: dict[str, tuple[str, str]], root: Path,
                            *, expect_cache_change: bool) -> None:
    after = tree_files(root)
    changed = {path for path in before.keys() | after.keys()
               if before.get(path) != after.get(path)}
    outside = sorted(path for path in changed if not is_summary_cache_path(path))
    if outside:
        raise AssertionError(f"query changed non-cache paths: {outside}")
    cache_changes = sorted(path for path in changed if is_summary_cache_path(path))
    if expect_cache_change and not cache_changes:
        raise AssertionError("cold remote query did not leave an observable summary-cache write")
    if any(path.startswith(("user-flatpak/app/", "user-flatpak/runtime/",
                           "system-flatpak/app/", "system-flatpak/runtime/"))
           for path in changed):
        raise AssertionError("candidate lookup changed an installed deployment")
    print(f"cache writes: {cache_changes or 'none'}")


def installed_commit(env: dict[str, str], scope: str, branch: str) -> str:
    return run(["flatpak", "info", "--show-commit", f"--{scope}", APP_ID, branch], env).stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--test-executable", required=True, type=Path)
    args = parser.parse_args()
    executable = args.test_executable.resolve(strict=True)

    version = run(["flatpak", "--version"], os.environ.copy()).stdout.strip()
    if version != "Flatpak 1.18.4":
        raise RuntimeError(f"expected Flatpak 1.18.4, got {version!r}")
    print(version)

    with tempfile.TemporaryDirectory(prefix="cosmos-flatpak-cli-") as temp:
        root = Path(temp)
        dirs = {
            "home": root / "home",
            "runtime": root / "runtime",
            "config": root / "config",
            "data": root / "data",
            "cache": root / "cache",
            "user": root / "user-flatpak",
            "system": root / "system-flatpak",
            "system_cache": root / "system-cache",
            "run": root / "run",
            "repo": root / "repo",
        }
        for path in dirs.values():
            path.mkdir(parents=True, exist_ok=True)
        dirs["runtime"].chmod(0o700)
        dirs["run"].chmod(0o700)
        (dirs["config"] / "installations.d").mkdir(parents=True)

        env = os.environ.copy()
        env.update({
            "HOME": str(dirs["home"]),
            "USER": "root",
            "XDG_RUNTIME_DIR": str(dirs["runtime"]),
            "XDG_CONFIG_HOME": str(dirs["config"]),
            "XDG_DATA_HOME": str(dirs["data"]),
            "XDG_CACHE_HOME": str(dirs["cache"]),
            "FLATPAK_CONFIG_DIR": str(dirs["config"]),
            "FLATPAK_DATA_DIR": str(root / "flatpak-data"),
            "FLATPAK_RUN_DIR": str(dirs["run"]),
            "FLATPAK_USER_DIR": str(dirs["user"]),
            "FLATPAK_SYSTEM_DIR": str(dirs["system"]),
            "FLATPAK_SYSTEM_CACHE_DIR": str(dirs["system_cache"]),
            "FLATPAK_SYSTEM_HELPER_ON_SESSION": "1",
            "LC_ALL": "C",
            "NO_PROXY": "127.0.0.1,localhost",
            "no_proxy": "127.0.0.1,localhost",
        })
        for key in ("http_proxy", "https_proxy", "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY"):
            env.pop(key, None)

        make_runtime(root, env, dirs["repo"])
        make_app(root, env, dirs["repo"], "stable", "old payload")
        make_app(root, env, dirs["repo"], "beta", "unchanged beta payload")
        run(["flatpak", "build-update-repo", "--no-update-appstream", str(dirs["repo"])], env)

        arch = run(["flatpak", "--default-arch"], env).stdout.strip()
        expected_runtime_ref = f"runtime/{RUNTIME_ID}/{arch}/stable"
        repo_refs = run(["flatpak", "repo", "--branches", str(dirs["repo"])], env).stdout
        if expected_runtime_ref not in repo_refs:
            raise AssertionError(
                f"synthetic OSTree repo is missing {expected_runtime_ref!r}: {repo_refs!r}"
            )

        server, thread, remote_url = start_local_repo_server(root)
        try:
            for scope in ("user", "system"):
                run(["flatpak", "remote-add", f"--{scope}", "--no-gpg-verify",
                     REMOTE, remote_url], env)
            remote_runtime_refs = run([
                "flatpak", "remote-ls", "--user", "--runtime", "--columns=ref", REMOTE,
            ], env).stdout
            if expected_runtime_ref not in remote_runtime_refs:
                raise AssertionError(
                    f"synthetic remote is missing {expected_runtime_ref!r}: "
                    f"{remote_runtime_refs!r}"
                )
            run(["flatpak", "install", "--user", "--assumeyes", REMOTE,
                 f"{APP_ID}//stable"], env)
            run(["flatpak", "install", "--user", "--assumeyes", REMOTE,
                 f"{APP_ID}//beta"], env)
            run(["flatpak", "install", "--system", "--assumeyes", REMOTE,
                 f"{APP_ID}//stable"], env)

            installed_before = {
                ("user", "stable"): installed_commit(env, "user", "stable"),
                ("user", "beta"): installed_commit(env, "user", "beta"),
                ("system", "stable"): installed_commit(env, "system", "stable"),
            }
            if installed_before[("user", "stable")] == installed_before[("user", "beta")]:
                raise AssertionError("fixture did not produce separate stable and beta refs")

            time.sleep(1.1)
            make_app(root, env, dirs["repo"], "stable", "new stable payload", reexport=True)
            run(["flatpak", "build-update-repo", "--no-update-appstream", str(dirs["repo"])], env)

            clear_summary_caches(root)
            cold_cached = run([
                "flatpak", "remote-ls", "--updates", "--app",
                "--columns=application,arch,branch,version,commit", "--user", "--cached", REMOTE,
            ], env, expect_success=False)
            if cold_cached.returncode == 0 or "No cached summary" not in cold_cached.stderr:
                raise AssertionError(
                    "cold --cached query did not reproduce the expected missing-summary failure: "
                    f"status={cold_cached.returncode}, stderr={cold_cached.stderr!r}"
                )
            print("cold --cached query: expected No cached summary failure")

            before = tree_files(root)
            run([str(executable), "candidate"], env)
            check_only_cache_writes(before, root, expect_cache_change=True)

            before_cached_run = tree_files(root)
            run([str(executable), "candidate"], env)
            check_only_cache_writes(before_cached_run, root, expect_cache_change=False)

            installed_after = {
                ("user", "stable"): installed_commit(env, "user", "stable"),
                ("user", "beta"): installed_commit(env, "user", "beta"),
                ("system", "stable"): installed_commit(env, "system", "stable"),
            }
            if installed_after != installed_before:
                raise AssertionError("candidate lookup changed an installed Flatpak commit")
            print("installed app commits: unchanged across candidate queries")

            server.shutdown()
            thread.join(timeout=5)
            server.server_close()
            clear_summary_caches(root)
            before_unavailable = tree_files(root)
            run([str(executable), "unavailable"], env)
            check_only_cache_writes(before_unavailable, root, expect_cache_change=False)
            if {key: installed_commit(env, *key) for key in installed_before} != installed_before:
                raise AssertionError("unavailable-source check changed an installed commit")
            print("unreachable local remote: Core/Qt reports unavailable; commits unchanged")
        finally:
            server.shutdown()
            thread.join(timeout=5)
            server.server_close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
