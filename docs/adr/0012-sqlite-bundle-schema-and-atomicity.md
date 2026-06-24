# 0012. SQLite `.yesdaw` bundle — schema v1, migrations, cross-file atomicity

- **Status:** Accepted
- **Date:** 2026-06-23
- **Deciders:** Dan (owner), build agent (H1)
- **Related:** ADR-0002 (SQLite-centered persistence), ADR-0010/0009/0011 (the time, event, and entity
  data this stores), ADR-0013 (plugin-state chunks, H3 — header reserved here),
  [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) decision #10 + fork #5,
  [deepening notes](../plans/2026-06-23-yes-daw-deepening-notes.md) → *Data model* and risks #3/#5/#12,
  `CONTEXT.md` (Project bundle, Project).

## Context

The Project bundle is the canonical, irreversible artifact: once users have `.yesdaw` files, the
on-disk shape and its integrity rules cannot be cheaply changed. SQLite is the transactional core
(fork #5: normalized tables, not JSONB). The hard parts are the ones that are near-impossible to
retrofit: **cross-file atomicity** (SQLite ACID covers only `project.db`, but import/delete/migration
all cross the DB↔filesystem seam — deepening-notes risk #3), real **referential integrity** from v1,
and a **migration** harness. The H1 exit criterion includes "a kill during save or migration reopens
cleanly," so these are designed now, not deferred.

## Options considered

1. **Single JSON/XML document file.** Rejected (fork #5): no transactions, no partial load, no
   migration story, rewrites the whole file on every save.
2. **SQLite with a single big JSONB blob.** Rejected: loses queryability and referential integrity;
   schema evolution becomes blob-surgery.
3. **SQLite bundle, normalized tables, WAL, intent-log for cross-file atomicity, numbered migrations.**
   - Pros: transactional, queryable, partial-load, real FKs, a clean migration path, crash-recoverable.
   - Cons: the most machinery to build. Accepted — it is the format.

## Decision

**A `.yesdaw` bundle is a package directory; only `project.db` (SQLite) is transactional.**

- **Connection bring-up order matters:** `journal_mode=WAL` → `synchronous=NORMAL` (live; escalate to
  `FULL` + `wal_checkpoint(TRUNCATE)` at explicit Save) → `foreign_keys=ON` → `busy_timeout=5000` →
  `wal_autocheckpoint=1000`. Freeze `application_id` once (`0x59455331` "YES1"); `user_version` is the
  monotonic schema version.
- **Cross-file atomicity (not covered by SQLite ACID): intent log + reconcile-on-open.** Every
  asset/BLOB write is **file-before-row**: stage to a temp name in the *same directory*, `fsync` file +
  dir, atomic-rename to `<hash>.<ext>`, then `COMMIT` the referencing row; record intended FS ops in a
  `pending_fs_ops` table inside the same transaction. On open, replay/rollback `committed=0` ops and
  sweep orphans. GC never hard-deletes — unreferenced assets move to `.trash/`, swept only on explicit
  "Clean up project."
- **Real referential integrity from v1:** `FOREIGN KEY` constraints + `foreign_keys=ON`. Layered checks
  on open: fast `quick_check`, background full `integrity_check`, then **our own** referential checks
  (no orphan refs) and **semantic invariants** (`clip.src_offset+len ≤ asset.frames`; monotonic finite
  tempo map; DAG acyclicity with depth/size cap before topo-sort; `time_base` enum range; positive
  PPQ/block/track counts). `integrity_check` validates B-tree structure only — a structurally-perfect DB
  can still be semantically corrupt.
- **Migrations: numbered, transactional, append-only, copy-on-migrate.** One `Migration{toVersion,
  apply}`; `BEGIN IMMEDIATE` → apply → insert `schema_migrations(version, applied_at_utc, app_build)` →
  bump `user_version` as the **last** statement → `COMMIT`. Never edit/delete a shipped migration; never
  drop columns/tables in the v1 era (add-and-deprecate). **Refuse-forward:** `user_version >
  CODE_SCHEMA_VERSION` opens read-only. File-moving migrations migrate a *copy* (Online Backup), validate
  it, then atomically swap — the original is untouched until the new one is proven good.
- **Durability, stated honestly:** `synchronous=NORMAL` + WAL survives a clean process kill but **not**
  power loss; explicit Save on macOS must issue **`F_FULLFSYNC`** (plain `fsync` doesn't reach the
  platter on Apple hardware). The UI distinguishes "autosaved (crash-recoverable)" from "saved
  (power-loss-durable)."
- **Plugin state is opaque chunks** (full detail at ADR-0013, H3): the bundle reserves a chunk table
  with a `{format, plugin_uid, plugin_version, chunk_kind, chunk_len, crc32}` header; never reconstruct
  state from parameter values. (Autosave/crash-recovery via the Online Backup API and DAWproject export
  are H6.)

## Consequences

- **Positive:** transactional, queryable, partial-loadable canonical format; crash during save/migration
  reopens clean (WAL recovery + reconcile-on-open); referential + semantic integrity from v1; a real
  forward-only migration path; soft-delete means no destructive GC.
- **Negative / accepted costs:** the intent-log/reconcile machinery and layered integrity checks are
  real code; power-loss durability needs an explicit barrier and `F_FULLFSYNC`; migrations are
  append-only forever (discipline cost).
- **Follow-ups:** CONTEXT.md Project-bundle entry already names `project.db` + assets + caches. H1 exit
  gate: kill during save/migration → reopen clean (`quick_check` + referential). Chaos-recovery suite
  (kill at randomized points; corrupt `project.db`/WAL/asset/plugin BLOB → detect-and-quarantine, never
  crash) is built at H1 per the testing strategy. Hardened open
  (`DEFENSIVE`/`trusted_schema=OFF`/authorizer) lands with the untrusted-input work.
