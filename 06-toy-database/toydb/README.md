# ToyDB — A B+Tree Key-Value Database Engine

[![Rust](https://img.shields.io/badge/rust-1.95+-orange.svg)](https://www.rust-lang.org)
[![Tests](https://img.shields.io/badge/tests-19%2F19-brightgreen.svg)](.)
[![Clippy](https://img.shields.io/badge/clippy-clean-brightgreen.svg)](.)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A progressively-built, production-pattern-aware database engine in Rust. Phase 1 delivers a fully functional in-memory B+Tree with insert, lookup, delete, range scan, and invariant checking.

> **Part of the [Tiny Projects](https://github.com/1998x-stack/tiny-projects) collection** — 8 toy CS projects from scratch.

---

## Quick Start

```bash
cd 06-toy-database/toydb

# Build
cargo build

# Run tests (19 tests, 100% passing)
cargo test

# Lint
cargo clippy
```

### Docker

```bash
cd 06-toy-database
docker build -t toydb .
docker run --rm -it -v $(pwd)/toydb:/workspace toydb
# Inside container: cargo test
```

---

## Architecture

```
┌─────────────────────────────────────────┐
│              BPlusTree                   │
│  lookup · insert · delete · range_scan  │
│  leaf_split · inner_split · invariants  │
└───────────────┬─────────────────────────┘
                │
┌───────────────┴─────────────────────────┐
│            MemoryPageStore               │
│  HashMap-backed page pool + freelist     │
└───────────────┬─────────────────────────┘
                │
┌───────────────┴─────────────────────────┐
│               Page (4096 bytes)          │
│  ┌─ Header (16B) ─────────────────────┐ │
│  │ type │ cells │ parent │ next │ csum│ │
│  └────────────────────────────────────┘ │
│  ┌─ Cell Data ────────────────────────┐ │
│  │ Inner: [key_len│key│child_page_id]  │ │
│  │ Leaf:  [key_len│key│val_len│value]  │ │
│  └────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

### Node capacity (derived from page layout)

| Node Type | Cell Size | Max Cells | Fan-out |
|-----------|-----------|-----------|---------|
| Inner | ~14 bytes | 290 | 290 children per node |
| Leaf  | ~268 bytes | 15 | 15 key-value pairs per node |

With 290 children per inner node, a 3-level tree stores **~1.26 million records**.

---

## API

```rust
use std::sync::Arc;
use toydb::storage::btree::BPlusTree;
use toydb::storage::mem_store::MemoryPageStore;

let mut tree = BPlusTree::new(Arc::new(MemoryPageStore::new()));

// Insert
tree.insert(b"user:1", b"Alice")?;
tree.insert(b"user:2", b"Bob")?;

// Lookup
assert_eq!(tree.lookup(b"user:1")?, Some(b"Alice".to_vec()));
assert_eq!(tree.lookup(b"user:3")?, None);

// Delete
tree.delete(b"user:1")?;

// Range scan
tree.insert(b"a", b"1")?;
tree.insert(b"b", b"2")?;
tree.insert(b"c", b"3")?;
let results = tree.range_scan(b"a", b"b")?;
assert_eq!(results.len(), 2);

// Invariant checks (for testing)
tree.verify_leaf_depth()?;   // all leaves at same depth
tree.verify_node_capacity()?; // no node overflows
```

---

## Project Structure

```
toydb/
├── Cargo.toml
└── src/
    ├── lib.rs              # Module declarations
    ├── error.rs            # DbError enum, Result alias
    ├── main.rs             # Placeholder entry point
    └── storage/
        ├── mod.rs
        ├── page.rs         # Page layout, cell I/O, lower_bound, checksums
        ├── mem_store.rs    # In-memory HashMap-backed page store
        └── btree.rs        # BPlusTree: all operations + invariant checks
```

---

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **Node capacity from layout** | No magic constants — order derived from `(4096 - 16) / cell_size` |
| **Inline byte reads** | No struct/serialization mismatch. All header fields accessed via `data[offset]` |
| **B+Tree variant** | Leaf chain for range scans; keys copied up on split (not removed) |
| **Allow underflow on delete** | Simplifies code — production DBs merge/redistribute, toy DB accepts sparse pages |
| **CRC32 checksums** | Two-pass hash over page data, skipping checksum field — zero allocation |
| **Zero-alloc lower_bound** | Binary search uses `leaf_key_at()` / `inner_key_at()` returning `&[u8]` without allocation |
| **Separator convention** | Inner node cell `(key, child)`: left subtree keys < key, right subtree keys >= key |

---

## Tests

```
running 19 tests
test storage::page::tests::test_checksum ................. ok
test storage::page::tests::test_inner_cell_insert_and_read ok
test storage::page::tests::test_leaf_cell_insert_and_read . ok
test storage::page::tests::test_leaf_cell_multiple_in_order ok
test storage::page::tests::test_lower_bound .............. ok
test storage::page::tests::test_lower_bound_empty ........ ok
test storage::mem_store::tests::test_allocate_and_read_back ok
test storage::mem_store::tests::test_free_and_reuse ...... ok
test storage::mem_store::tests::test_read_nonexistent_page ok
test storage::btree::tests::test_empty_tree_lookup ....... ok
test storage::btree::tests::test_insert_single_and_lookup  ok
test storage::btree::tests::test_lookup_missing_key ...... ok
test storage::btree::tests::test_leaf_split .............. ok
test storage::btree::tests::test_root_split .............. ok
test storage::btree::tests::test_delete .................. ok
test storage::btree::tests::test_delete_nonexistent ...... ok
test storage::btree::tests::test_range_scan .............. ok
test storage::btree::tests::test_range_scan_across_leaves  ok
test storage::btree::tests::test_invariants_after_random_ops ok

test result: ok. 19 passed; 0 failed
```

---

## Roadmap

| Phase | Feature | Status |
|-------|---------|--------|
| 1 | In-memory B+Tree (single-threaded) | ✅ Done |
| 2 | Buffer Manager + WAL + Crash Recovery | 🔜 Planned |
| 3 | Concurrency — Latch Coupling | 🔜 Planned |
| 4 | MVCC Transactions | 🔜 Planned |
| 5 | SQL Frontend | 🔜 Planned |
| 6 | Raft Replication | 🔜 Planned |

---

## References

- [SQLite btree.c](https://www.sqlite.org/src/file/src/btree.c) — Production B-Tree implementation
- [PostgreSQL nbtree](https://github.com/postgres/postgres/tree/master/src/backend/access/nbtree) — Concurrency patterns
- [Database System Concepts (7th ed)](https://www.db-book.com/) — B+Tree algorithms
- [Let's Build a Simple Database](https://cstack.github.io/db_tutorial/) — SQLite clone tutorial

## License

MIT
