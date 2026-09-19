# Verification

How to reproduce this repository's verified test results yourself, and
how to read the output when something doesn't pass.

## Prerequisites

- `bash`
- a C++20-capable `g++` on `PATH`
- GNU binutils (`as`, `ld`) — the OGLang end-to-end tests assemble and
  link real native binaries; these normally ship alongside `g++`

No other tools, containers, or package installs are required. There is
currently no Dockerfile/container image for this project — verification
runs directly against whatever host or VM toolchain you have.

## Supported environment

Any POSIX environment with the prerequisites above works: native
Linux, macOS with the Xcode command-line tools, or Windows via WSL2.

**Run from a native filesystem.** On Windows, prefer a path inside the
WSL2 distro's own filesystem (e.g. `~/projects/...` or `/tmp/...`)
rather than a Windows-mounted path (`/mnt/c/...`). See the caveat
below for why.

## Running verification

```sh
bash tools/verify_all.sh
```

This builds and runs every hosted (no emulator required) test suite
across the repository and prints one line per suite plus a real,
aggregated total — never a fabricated or rounded number.

### What it covers

OGLang lexer/parser/type-checker/codegen (unit + end-to-end native
binaries), the OS's hosted-only subsystems (kernel heap, ELF32 loader,
keyboard-translation table), the networking protocol codecs, the
distributed-systems RPC/fault-injection/Raft/authenticated-envelope
suites, the write-ahead-log + KV store, SHA-256 and HMAC-SHA256,
cross-layer property-based tests, OGGit's object store, refs/HEAD
history, index/staging area, checkout, and diff, the quantum
state-vector simulator, and the scientific-
computing + orbital-mechanics numerics. See `PROJECT_STATE.md` for the
authoritative, per-capability breakdown with evidence links.

### What it does NOT cover

- `22-os/tests/boot_test.sh` and `22-os/tests/keyboard_test.sh` —
  these boot a real kernel image under QEMU and inject real scancodes;
  they need `qemu-system-i386` and take much longer, so `verify_all.sh`
  intentionally does not run them. Run them directly once you have
  QEMU installed:

  ```sh
  bash 22-os/tests/boot_test.sh
  bash 22-os/tests/keyboard_test.sh
  ```

  Both scripts already skip themselves gracefully (rather than
  failing) when `qemu-system-i386` isn't on `PATH`.

- Any PRD layer with no entry in `verify_all.sh`'s suite list has no
  automated test suite yet — the script prints this explicitly in its
  own "Layers with NO test suite" section rather than silently
  omitting it. As of this writing that's layers 0-2, 9, 12, 14,
  16-17, 19, and 21; see `PROJECT_STATE.md` for the current, most
  accurate list.

## Interpreting failures

`verify_all.sh` classifies every suite result into one of:

- **PASS** — the suite built, ran, and every assertion passed.
- **FAIL — CODE/TEST FAILURE** — the suite built and ran, but at least
  one real assertion inside it failed. This is a genuine defect.
- **FAIL — BUILD/TOOLCHAIN FAILURE** — the suite's compiler/linker
  invocation itself failed (a syntax error, a missing header, an
  incompatible flag), so no assertions ever ran. The failing
  compiler/linker output is printed so the exact cause is visible.
- **ENVIRONMENT ERROR** — a preflight check confirmed the suite's own
  `tests/` directory cannot accept a newly created file at all, so the
  compiler was never even invoked. This is never counted as a code or
  test failure. See the WSL caveat below — this is the condition that
  causes it in practice.
- **TOOLCHAIN ERROR** — printed once, up front, and the whole run
  stops immediately, if no `g++` is found on `PATH` at all.

The final `RESULT:` line at the bottom distinguishes "all suites
passed" from "verification incomplete due to environment error(s), no
code failures observed" from "at least one suite genuinely failed" —
these are different situations and are never collapsed into one
generic failure message.

## WSL2 `/mnt/c` filesystem caveat

Every test script here builds a native test binary directly inside its
own `tests/` directory (e.g. `10-cryptography/tests/sha256_test_bin`).
Running `tools/verify_all.sh` from a WSL2 shell against a
Windows-mounted path (`/mnt/c/Users/.../THE-OG-TECHUILAGUY`) can hit a
DrvFs 9p permission quirk where some of those directories end up not
writable to the invoking user even though the files already inside
them are — `ld` then fails with `Permission denied` while trying to
create the output binary, for suites that are otherwise completely
correct.

`verify_all.sh` preflights every suite's test directory for real write
access (creating and removing a temp file, not just inspecting
permission bits) and reports this condition as an **ENVIRONMENT
ERROR**, distinct from a code or test failure, with the affected path
printed. If you see this, the fix is not to debug the code — it's to
run verification from a native Linux filesystem instead:

```sh
rm -rf /tmp/og-techuilaguy-verify
git clone <this-repo-url> /tmp/og-techuilaguy-verify
cd /tmp/og-techuilaguy-verify
bash tools/verify_all.sh
```

This is a filesystem-mount characteristic, not something specific to
any one machine, username, or WSL distribution — any WSL2 distro
exhibits it the same way for a Windows-mounted path.

## Getting a clean checkout for reproducibility

To reproduce the currently-reported result from scratch, verify
against a fresh clone rather than a working copy that may have
uncommitted changes:

```sh
rm -rf /tmp/og-techuilaguy-verify
git clone <this-repo-url> /tmp/og-techuilaguy-verify
cd /tmp/og-techuilaguy-verify
git rev-parse HEAD
bash tools/verify_all.sh
```

Compare the printed commit hash and the `TOTAL:` line against what
`PROJECT_STATE.md`/`README.md` report for that commit.
