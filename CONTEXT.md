# CONTEXT — Toy Database Domain Terms

> Resolved during grill-with-docs session on 2026-05-18.
> These terms are canonical for the Toy Database project.

## Glossary

| Term | Definition |
|------|------------|
| **B+Tree Index Key** | Raw byte key stored in B+Tree inner/leaf nodes. In Phase 1-4, caller provides this directly. In Phase 5, the SQL layer constructs it as `[table_id: u16][primary_key_bytes]`. |
| **Primary Key** | SQL-level concept — the column(s) declared `PRIMARY KEY` in DDL. Mapped to B+Tree index key by the SQL layer. |
| **Table ID** | A 2-byte prefix added to every B+Tree key to namespace tables within a single B+Tree instance. System catalog uses table_id=0. |
| **Logical WAL** | Write-Ahead Log that records operations (`Put`, `Delete`, `Commit`) rather than physical page images. Compact. Recovery replays operations on the live B+Tree. |
| **LSN** (Log Sequence Number) | Monotonically increasing identifier for WAL entries. Used for checkpoint tracking, version chain pointers, and Raft log indexing. |
| **Page Latch** | Short-lived lock on a B+Tree page, held during access and released immediately. Prevents physical corruption. Distinct from transaction locks. |
| **Snapshot** | Frozen view of the database at a point in time. Taken at `begin()`. Contains `xmin` (oldest active txn), `xmax` (next txn ID), and `active_list`. |
| **First-Committer-Wins** | Conflict resolution strategy: if two concurrent transactions write the same key, the first to commit succeeds. The second detects the conflict at commit time and must abort/retry. Same model as PostgreSQL Repeatable Read. |
| **Write-Set** | Set of keys modified by a transaction. Checked at commit time against committed txns that ran after the snapshot was taken. |
| **Write-Write Conflict** | Error raised when a transaction's write-set overlaps with a transaction that committed after its snapshot. |
| **Hybrid Version Cache** | Storage model where the B+Tree value holds the last 2 versions inline (fast current-state + recent-history access), while older versions exist only in WAL entries, reachable via `prev_lsn`. |
| **Tombstone** | A version with `value = None`, indicating the key was deleted. Filtered out during range scans. Physically removed from the B+Tree by GC when no active snapshot can see any prior version. |
| **Checkpoint** | Process of replaying committed WAL entries into the main B+Tree, then marking the WAL position so entries before it can be truncated. Must run BEFORE GC. |
| **Garbage Collection** (GC / Vacuum) | Process of removing versions and WAL entries no longer visible to any active or future transaction. Runs AFTER checkpoint. Removes tombstones physically from the B+Tree. |
| **Raft Log** | The WAL serves as the Raft log. Each WAL entry is a Raft log entry. Raft commit = WAL fsync. Unifies crash recovery and replication into a single durable log. |
| **Deterministic Replay** | Raft followers apply log entries by directly setting values (not re-executing `write()`), ensuring byte-identical state with the leader. Log entries carry both `old_value` (for verification) and `new_value` (for application). |

## Key Design Decisions

1. **B+Tree order is derived from page layout**, not user-configurable. Test-only override via builder pattern.
2. **Logical WAL** — logs operations, not pages. Compact, but recovery replays on live B+Tree.
3. **First-committer-wins** conflict resolution (Postgres-style optimistic concurrency).
4. **Hybrid version cache** — 2 inline versions + WAL for older.
5. **Single B+Tree with table_id prefix** for multi-table namespacing.
6. **Snapshot frozen at begin()** — Repeatable Read isolation.
7. **Checkpoint must run before GC** — prevents GC from removing uncheckpointed data.
8. **Raft entries carry both old_val + new_val** for deterministic follower replay.
