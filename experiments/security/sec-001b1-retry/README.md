# SEC-001B1 deterministic synthetic probe retry

This folder contains an **unprivileged, synthetic-only observer fixture** for one bounded rerun of the SEC-001B1 process-path experiment. It does **not** sandbox DSH, enforce host permissions, trace unrelated processes or demonstrate operating-system-level isolation.

## Previous failure and fix

The first attempt double-appended a newline to a fixture and exited before observation. The corrected `verify_fixture` accepts exactly: no terminator, one LF or one CRLF. All other inputs, including double terminators, fail before output creation.

A normal run first writes a private `phase=ready` result, prints a flushed `SEC001B1_READY` marker, and holds for an explicit 0–30 second observation interval. It then atomically updates to `phase=completed`. Existing `result.json` prevents a silent duplicate run.

## Offline tests (safe; no DSH or Relay)

    cd experiments/security/sec-001b1-retry
    python3 -m unittest -v test_probe.py

9 unit tests passed in an independent environment. They are **not** a real DSH execution-chain result.

## Target-host supervised probe (not run by this PR)

Codex prepares a fresh synthetic temporary directory containing `fixture.txt` with the literal bytes `sec-001b1-synthetic-v1` followed by at most one LF or CRLF. After independent offline tests, Codex may issue one bounded synthetic task to the already-connected Relay route with **exact read/write/command scopes**; it must not restart Host or scan all prior sessions.

Only the unique probe during its READY interval should be sampled. Codex keeps PID, parent PID, namespaces and cgroup metadata **locally and privately**. Relay `runComplete=true` without an independently confirmed native session terminal state is `unverified`, not proof of successful handoff. Failure must not trigger a silent second run.

A successful probe still does not prove actual DSH file-access tools use a sandbox. SEC-001 remains open and unattended continuous write remains disabled until the full execution chain and OS-enforced denial tests pass.

No credentials, Relay addresses, private paths, real package commands, or visual assets are included.
