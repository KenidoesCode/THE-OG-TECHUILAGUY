# ADR 0023: OGForge server foundation — repositories, auth, push, browse

**Status:** Accepted. This is the FOUNDATION vertical slice of
OGForge, PRD Layer 13's developer-platform half of "OGGit, OGForge,
OGJudge, package registry." It builds a real server-logic library on
top of the existing, real OGGit implementation (ADR 0007/0014/0017/
0018/0019/0020) — repository creation, password authentication,
pushing real objects, moving branches, and public browsing — all with
real on-disk persistence. It is explicitly not a network-facing HTTP/
RPC server yet, not issues/PRs/CI/registry, and not a frontend.

## Context

OGGit (object store, refs, index, checkout, diff, merge) is real and
tested, but it is a local, single-repository library — nothing lets a
second party discover, authenticate against, or push to a repository
someone else hosts. OGForge is that missing layer. The smallest real
foundation is: multiple named repositories, each a real OGGit
repository on disk; a real (if minimal) authentication mechanism; and
operations (create, push, browse) that a future network transport can
sit in front of without needing to change any of this logic.

## Architecture

`ForgeServer` (`13-developer-ecosystem/forge/forge_server.hpp/.cpp`)
is an in-process server-logic library — **not yet bound to any socket,
HTTP, or RPC transport**. Every "request" is a direct C++ call. This
is a deliberate scope boundary, not an oversight: `07-distributed-
systems/rpc/rpc.hpp` already has a real request/response/serialization
foundation (ADR 0008); wiring `ForgeServer`'s operations behind that
transport is the natural next integration step, kept separate so this
ADR's actual server *logic* can be built and tested in isolation first.

Each repository `ForgeServer` manages is a real, independent OGGit
repository: `ForgeServer` constructs a real `oggit::ObjectStore` and
`oggit::RefStore` rooted at `<forgeRoot>/repos/<repoName>/` per
operation (both are cheap, directory-backed, stateless-between-calls
classes already — no new repository storage format is introduced).
Creating a repository is exactly "these directories now exist";
pushing an object is exactly "`ObjectStore::writeObject` was called
against this repository's root"; browsing is exactly reading them back
through the same, already-tested OGGit classes.

## Authentication

A `ForgeServer` maintains a user table (`username -> SHA-256(password)`
digest), persisted as a single file at `<forgeRoot>/users.db` (a
simple, this-project's-own binary format via `07-distributed-systems`'
existing `Encoder`/`Decoder` — real cross-layer reuse, not a third
serializer) and reloaded on construction.

**This is explicitly not production-grade authentication.** Plain
`SHA-256(password)` with no salt and no slow/memory-hard KDF (bcrypt/
scrypt/Argon2) is vulnerable to rainbow-table and brute-force attacks
at real-world scale — it exists here only to make `createRepository`/
`pushObject`/`setBranch` require *a* credential check with a real,
testable pass/fail outcome, establishing the *shape* of an
authenticated write path. A real KDF-based credential store (or better,
a real PQC/asymmetric-signature-based identity, which the project's
own crypto direction eventually calls for) is explicit future work —
see "What this does not support."

## Operations

- `registerUser(username, password)`: rejects an empty username or a
  username that already exists.
- `authenticate(username, password)`: constant-effort-independent
  simple comparison (this slice does **not** claim the constant-time
  guarantee `10-cryptography/mac/hmac_sha256.hpp`'s
  `constantTimeEquals` provides elsewhere in this project — a real gap,
  named explicitly below, not hidden).
- `createRepository(username, password, repoName)`: requires
  successful authentication; rejects an empty `repoName` or one that
  already exists (checked by real directory existence, not an
  in-memory registry that could drift from disk).
- `pushObject(username, password, repoName, type, content)`: requires
  authentication and an existing repository; writes a real OGGit
  object (blob/tree/commit) via that repository's real `ObjectStore`
  and returns its real content-addressed id.
- `setBranch(username, password, repoName, branchName, commitId)`:
  requires authentication, an existing repository, **and** that
  `commitId` already exists as a real object in that repository's
  store (rejecting a branch pointed at an object that was never
  pushed) — then moves the branch via the real `RefStore`.
- `listBranches(repoName)` / `getObject(repoName, id)`: **no
  authentication required** — repository browsing is public, matching
  every real code-forge's default (private repositories are explicit
  future work, not the default here either).

## Tested invariants

`13-developer-ecosystem/tests/forge_server_test.cpp`: creating a
repository after successful registration/authentication succeeds and
is independently visible on disk; creating a repository with a wrong
password fails with `AuthenticationFailed` and creates nothing;
creating a repository that already exists fails with
`RepositoryAlreadyExists` without touching the existing one; looking up
a nonexistent repository's branches/objects fails cleanly with
`RepositoryNotFound`, not a crash; pushing a real blob/tree/commit
succeeds, and the exact bytes are retrievable afterward via
`getObject` — a genuine round trip through real OGGit storage, not a
mock; `setBranch` succeeds only when the target commit id was actually
pushed first, and fails cleanly (not silently) otherwise; pushing to
or creating a repository with an empty name is rejected as
`InvalidRequest`; a corrupted on-disk object (the same re-hash-on-read
detection ADR 0007 already established) is reported through
`getObject` as `ObjectNotFound` rather than returning wrong content;
and — the real persistence proof — a second, independent `ForgeServer`
instance constructed over the same `forgeRoot` after the first is
destroyed recovers every registered user (authentication still
succeeds) and every previously-pushed object/branch remains fully
readable, a genuine process-restart simulation.

## What this does not support

- **No network transport.** Every operation is a direct C++ call; no
  socket, HTTP endpoint, or RPC binding exists yet. Wiring this behind
  `07-distributed-systems/rpc`'s existing `RpcServer`/`RpcClient` is
  the explicit next integration step.
- **No real password security.** Unsalted `SHA-256(password)`, not a
  slow/memory-hard KDF; `authenticate`'s comparison is not
  constant-time. Neither is acceptable for a real deployment — see
  "Authentication" above.
- **No private repositories, no permissions/roles beyond
  "authenticated user can push to any repository they successfully
  authenticate against."** There is no per-repository ownership/ACL
  model.
- **No issues, pull requests, code review, CI, releases, package
  registry, webhooks, search, or developer profiles.** This ADR is the
  repository/auth/push/browse foundation those would all be built on.
- **No index/staging/checkout workflow exposed through the server** —
  a client pushes already-built objects (blobs/trees/commits) directly;
  there is no server-side "clone, edit a working tree, commit" flow
  (that already exists client-side via ADR 0017/0018, just not
  connected to a push protocol yet).
- **No git-compatible wire protocol.** Pushing here means calling
  `pushObject`/`setBranch` directly; there is no `git push`-compatible
  negotiation (want/have, packfiles) — OGGit itself doesn't have
  packfiles yet either (ADR 0007's scope).

## Consequences

THE OG TECHUILAGUY now has a real, tested, persistent multi-repository
server-logic layer on top of OGGit — genuinely "create a repository,
authenticate, push a real object, browse it back, restart the process,
confirm everything survived," the exact minimum end-to-end target this
slice was scoped to. The next real gaps, in roughly increasing order:
binding this behind the existing RPC transport so it's actually
reachable from a separate process, a real KDF-based credential store,
then progressively issues/PRs/CI/registry — none of which exist yet.
