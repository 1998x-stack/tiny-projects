# Toy Database — Enhanced Specification

> A progressive-build mini-database in Rust: B+Tree → MVCC → SQL → Distribution
>
> References: SQLite (btree.c, pager.c, wal.c), PostgreSQL (nbtree, heapam), toydb, baobab, boltdb, CMU Bustub

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Phase 1: In-Memory B+Tree](#phase-1-in-memory-btree)
3. [Phase 2: Persistence — Buffer Manager + WAL](#phase-2-persistence--buffer-manager--wal)
4. [Phase 3: Concurrency — Latch Coupling](#phase-3-concurrency--latch-coupling)
5. [Phase 4: MVCC Transactions](#phase-4-mvcc-transactions)
6. [Phase 5: SQL Frontend](#phase-5-sql-frontend)
7. [Phase 6: Distribution — Raft Replication](#phase-6-distribution--raft-replication)
8. [Rust Crate Structure](#rust-crate-structure)
9. [Development Roadmap](#development-roadmap)
10. [Success Criteria](#success-criteria)

---

## Architecture Overview

The database is built **progressively** across 6 phases. Each phase produces a working system that gains one capability. All layers are implemented in Rust.

```
                              ┌──────────────────────────┐
Phase 6: Distribution ───────►│ Raft Consensus / Gossip  │
                              └──────────┬───────────────┘
                                         │
                              ┌──────────┴───────────────┐
Phase 5: SQL Frontend ───────►│ Parser → Planner → Exec  │
                              └──────────┬───────────────┘
                                         │
                              ┌──────────┴───────────────┐
Phase 4: MVCC Transactions ──►│ Snapshot Isolation, GC   │
                              └──────────┬───────────────┘
                                         │
        ┌────────────────────────────────┼──────────────────────────────┐
        │                                │                              │
Phase 3 │ Concurrency ────────► Latch Coupling + Deadlock Prevention    │
Phase 2 │ Persistence ────────► Buffer Manager + WAL + Crash Recovery   │
Phase 1 │ Core B+Tree ────────► In-Memory B+Tree (Single-Threaded)      │
        └────────────────────────────────────────────────────────────────┘
```

**Module dependency graph:**
```
BTree (Phase 1)          ← no dependencies
  ├── BufferMgr (Phase 2) ← depends on BTree page abstraction
  ├── WAL (Phase 2)       ← depends on DiskMgr
  ├── LatchMgr (Phase 3)  ← injected into BTree (trait-based)
  ├── MVCC (Phase 4)      ← wraps BTree, uses WAL for commit log
  ├── SQL (Phase 5)       ← depends on MVCC engine
  └── Raft (Phase 6)      ← depends on WAL for log replication
```

**Design principles:**
- **Traits for abstraction** — `PageStore`, `EvictionPolicy`, `Engine`, `PageLatch` all defined as traits, enabling test doubles
- **Type safety** — Page types as Rust enums, `Result<T, DbError>` everywhere, no nullable raw pointers
- **Production patterns, toy scope** — study SQLite/PostgreSQL internals, apply simplified versions
- **Always working** — each phase produces a compilable, testable binary

---

## Phase 1: In-Memory B+Tree

### Goal

A working single-threaded B+Tree that can insert, lookup, delete, and range-scan key-value pairs entirely in memory. No persistence yet — data is lost on process exit.

### Success Criteria
- [ ] Insert 100K random KV pairs, verify all retrievable
- [ ] Delete a random 20%, verify remaining keys intact, deleted return None
- [ ] Range scan (`scan(start, end)`) returns correctly ordered results across leaf boundaries
- [ ] B+Tree invariants hold: all leaves at same depth, node key count between ⌈m/2⌉ and m
- [ ] Root split handled correctly (tree grows taller)

### Page Layout and Node Capacity

Pages are fixed-size 4096 bytes. The layout is inspired by SQLite's btree page format, simplified.

The B+Tree's node capacity ("order") is **derived from the page layout**, not configurable by the caller:

```rust
const HEADER_SIZE: usize = 16;
const INNER_CELL_SIZE: usize = 2 + 8 + 4;   // key_len(u16) + key(8B avg) + child_page_id(u32) = 14
const LEAF_CELL_SIZE: usize = 2 + 8 + 2 + 256; // key_len + key + value_len + value(256B avg) = 268
const INNER_ORDER: usize = (PAGE_SIZE - HEADER_SIZE) / INNER_CELL_SIZE;  // ~290
const LEAF_ORDER: usize = (PAGE_SIZE - HEADER_SIZE) / LEAF_CELL_SIZE;    // ~15
```

For testing (e.g., artificially small nodes to trigger splits with fewer keys), use a test-only constructor that accepts an explicit order override.

```rust
const PAGE_SIZE: usize = 4096;

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum PageType {
    Inner = 0,
    Leaf  = 1,
}

// On-disk header: 16 bytes
#[repr(C)]
struct PageHeader {
    page_type: u8,           // PageType discriminant
    num_cells: u16,          // number of key-value or key-child pairs
    parent_page_id: u32,     // 0 for root
    next_leaf: u32,          // leaf only: next leaf page ID for range scans
    checksum: u32,           // CRC32 of page content (validated on read)
    _padding: [u8; 2],
}
// assert_eq!(size_of::<PageHeader>(), 16);

// Remaining 4080 bytes: cell data, growing from both ends
// Inner node:  [header][cells → ← free space]
// Leaf node:   [header][cells → ← free space]
```

**Cell layout (variable-length, packed into page body):**

```
Inner cell: [key_len: u16][key_bytes: N bytes][child_page_id: u32]
Leaf cell:  [key_len: u16][key_bytes: N bytes][value_len: u16][value_bytes: M bytes]
```

Cells are stored in sorted order by key. Insertion shifts cells to make room — since toy DB uses small page sizes, `O(n)` shift per insert is acceptable.

### Data Structures

```rust
/// Page ID: unique identifier for a page on disk or in buffer pool
pub type PageId = u32;

/// Key and value are byte slices — no schema enforcement at this layer.
/// In Phase 1-4, the caller provides raw keys (the B+Tree is a generic K/V store).
/// In Phase 5, the SQL layer maps PRIMARY KEY columns to B+Tree keys, prefixed
/// with a table_id for namespace separation.
pub type Key = Vec<u8>;
pub type Value = Vec<u8>;

/// In-memory representation of a page
pub struct Page {
    pub id: PageId,
    pub data: [u8; PAGE_SIZE],
    pub is_dirty: bool,
}

impl Page {
    pub fn header(&self) -> &PageHeader { /* cast first 16 bytes */ }
    pub fn header_mut(&mut self) -> &mut PageHeader { /* cast first 16 bytes */ }

    pub fn page_type(&self) -> PageType { /* read header.page_type */ }

    /// Number of cells currently stored
    pub fn num_cells(&self) -> u16 { self.header().num_cells }

    /// Does this node have room for one more cell?
    pub fn has_space(&self, order: usize) -> bool {
        (self.num_cells() as usize) < order
    }

    /// For inner nodes: get child page ID at index
    pub fn child_at(&self, idx: usize) -> PageId { /* read from cell data */ }

    /// For leaf nodes: get value at index
    pub fn value_at(&self, idx: usize) -> &[u8] { /* read from cell data */ }

    /// Get key at index (both inner and leaf)
    pub fn key_at(&self, idx: usize) -> &[u8] { /* read from cell data */ }

    /// Binary search: first index where key_at(idx) >= target_key
    pub fn lower_bound(&self, target_key: &[u8]) -> usize { /* ... */ }
}
```

```rust
/// A version of a key's value
struct Version {
    txn_id: TxnId,                 // transaction that created this version
    value: Option<Vec<u8>>,        // None = tombstone (deleted)
    prev_lsn: Option<LSN>,         // pointer to previous version in WAL
    created_at: Instant,           // for GC age-based eviction
}

/// Hybrid cache: stores last N versions inline, older in WAL only.
/// N=2 for our toy DB: [current_version, previous_version].
/// Older versions retrieved from WAL via prev_lsn chain.
struct VersionCache {
    inline_versions: Vec<Version>,  // max 2, newest first
    has_older_in_wal: bool,         // true if more versions exist in WAL
}

/// Write-set for first-committer-wins conflict detection
struct WriteSet {
    keys: HashSet<Vec<u8>>,          // keys modified by this transaction
}

/// Abstraction over page storage — in-memory for Phase 1, on-disk for Phase 2
pub trait PageStore: Send + Sync {
    fn read_page(&self, id: PageId) -> Result<Page>;
    fn write_page(&self, page: &Page) -> Result<()>;
    fn allocate_page(&self) -> Result<PageId>;
    fn free_page(&self, id: PageId) -> Result<()>;
}

/// In-memory page store for Phase 1 testing
pub struct MemoryPageStore {
    pages: RwLock<HashMap<PageId, Page>>,
    next_id: AtomicU32,
    freelist: Mutex<Vec<PageId>>,
}

// Error type used throughout the system
pub enum DbError {
    PageNotFound(PageId),
    KeyNotFound,
    PageFull,
    ChecksumMismatch(PageId),
    IoError(std::io::Error),
}
```

### Core Algorithms

**Lookup:**

```rust
impl BPlusTree {
    pub fn lookup(&self, key: &[u8]) -> Result<Option<Value>> {
        if self.root_page_id == 0 {
            return Ok(None);  // empty tree
        }

        let mut page = self.page_store.read_page(self.root_page_id)?;

        // Traverse inner nodes to leaf
        while page.page_type() == PageType::Inner {
            let idx = page.lower_bound(key);
            // lower_bound returns first key >= search_key.
            // For inner nodes, the child to follow is:
            //   key < keys[0]        → idx=0, child=children[0]
            //   keys[i] <= key < keys[i+1] → child=children[i+1]
            //   key >= keys[last]    → child=children[last+1] = idx
            let child_id = page.child_at(idx);
            page = self.page_store.read_page(child_id)?;
        }

        // At leaf — exact match
        let idx = page.lower_bound(key);
        if idx < page.num_cells() as usize && page.key_at(idx) == key {
            Ok(Some(page.value_at(idx).to_vec()))
        } else {
            Ok(None)
        }
    }
}
```

**Insert with split propagation:**

```
Algorithm: insert(key, value)

1. If tree is empty (root_page_id == 0):
   → Create first leaf page as root, insert cell, return.

2. Traverse to leaf, collecting path stack:
   path = [(page_id, page), (page_id, page), ...]

3. If leaf has space:
   → Insert cell, write page, return.

4. Leaf is FULL → split:
   left_leaf, right_leaf, separator_key = split_leaf(leaf, key, value)

5. Propagate split upward:
   while path not empty:
       (parent_id, parent) = path.pop()
       if parent.has_space():
           parent.insert_separator(separator_key, right_child_id)
           write parent, left, right; return
       else:
           // Parent also full — recursive split
           left_parent, right_parent, new_sep = split_inner(parent, sep, child_id)
           separator_key = new_sep
           // continue propagating

6. Root was split — tree grows taller:
   new_root = new inner page with [separator_key], [left_child, right_child]
   tree.root_page_id = new_root.id
```

**Split mechanics (critical detail):**

For a node with `order` max cells (full), split at `mid = order / 2`:

- **Leaf split:** Left gets cells[0..mid], right gets cells[mid..]. Separator key = `right.keys[0]` — this key is COPIED up to parent (stays in right leaf).
- **Inner split:** Left gets cells[0..mid], right gets cells[mid+1..]. Separator key = `cells[mid].key` — this key is MOVED up to parent (removed from both children).

```
Example — Leaf split (order=4, mid=2):
Before: [A B C D]  (4 cells, full)
After:  Left=[A B], Right=[C D], Separator=C (copied up)

Example — Inner split (order=4, mid=2):
Before: [A B C D] → [c0 c1 c2 c3 c4]
After:  Left=[A B]→[c0 c1 c2], Right=[D]→[c3 c4], Separator=C (moved up)
```

**Delete (simplified — allow underflow):**

```
Algorithm: delete(key)

1. Traverse to leaf (same as lookup).
2. Find key in leaf. If not found, return Ok.
3. Remove cell from leaf, shifting remaining cells left.
4. Allow underflow (node may have fewer than ⌈order/2⌉ keys).
   This is a deliberate simplification — production B+Trees merge or
   redistribute underfull nodes. Our toy DB accepts space waste.
```

**Range scan (leaf chain traversal):**

```rust
pub fn range_scan(&self, start: &[u8], end: &[u8]) -> Result<Vec<(Key, Value)>> {
    let mut results = Vec::new();

    // Find starting leaf
    let mut page = self.find_leaf(start)?;

    loop {
        for i in 0..page.num_cells() as usize {
            let key = page.key_at(i);
            if key > end {
                return Ok(results);  // past end of range
            }
            if key >= start {
                results.push((key.to_vec(), page.value_at(i).to_vec()));
            }
        }

        // Advance to next leaf via chain
        let next_id = page.header().next_leaf;
        if next_id == 0 {
            break;  // end of leaf chain
        }
        page = self.page_store.read_page(next_id)?;
    }
    Ok(results)
}
```

### Page Size and Fan-out Analysis

With 4KB pages and 8-byte keys + 8-byte child pointers (inner nodes):
- Each inner cell: ~2 + 8 + 4 = 14 bytes → ~290 cells per page
- Each leaf cell: ~2 + 8 + 2 + 256 = ~15 entries per page (with 256-byte values)

Storage capacity with order 290:
| Height | Max Leaves | Max Records (256B values) |
|--------|-----------|--------------------------|
| 1 | 1 (root leaf) | ~15 |
| 2 | 290 | ~4,350 |
| 3 | 290² = 84,100 | ~1.26 million |
| 4 | 290³ = 24.4M | ~366 million |

For a toy database, height 2-3 is typical. A tree only 3 levels deep can store over 1 million records.

### Tests

```rust
#[cfg(test)]
mod tests {
    use super::*;

    fn setup_tree(order: usize) -> BPlusTree { /* ... */ }

    #[test]
    fn test_insert_and_lookup_single() { /* 1 key → found */ }

    #[test]
    fn test_insert_causes_leaf_split() { /* fill leaf → split → both leaves have keys */ }

    #[test]
    fn test_insert_causes_root_split() { /* fill root → new root created → height +1 */ }

    #[test]
    fn test_range_scan_across_leaves() { /* scan spans 3+ leaves → all keys in range */ }

    #[test]
    fn test_delete_and_reinsert() { /* delete → key gone; reinsert → key back */ }

    #[test]
    fn test_fanout_grows_with_data() { /* insert N keys → verify max depth matches log */ }

    #[test]
    fn test_invariants_after_random_ops() { /* random inserts/deletes → verify invariants */ }
}
```

---

## Phase 2: Persistence — Buffer Manager + WAL

### Goal

Data survives process restart. Writes go to durable storage. Crash recovery restores the last committed state.

### Success Criteria
- [ ] Insert data, restart process, verify data is still there
- [ ] Buffer pool evicts pages correctly under memory pressure
- [ ] Kill process mid-write → restart → WAL recovery restores to last committed state
- [ ] No silent data corruption (checksum catches bit flips)
- [ ] `fsync` called on commit (verify with strace/dtrace)

### Buffer Manager

The buffer pool caches frequently-accessed pages in memory, reducing disk I/O.

```rust
pub struct BufferPool {
    frames: Vec<BufferFrame>,          // fixed-size pool
    capacity: usize,
    page_table: RwLock<HashMap<PageId, usize>>,  // page_id → frame index
    evictor: Mutex<Box<dyn EvictionPolicy>>,
    disk: Arc<dyn DiskManager>,
}

struct BufferFrame {
    page: RwLock<Page>,                // the page data
    pin_count: AtomicU32,              // >0 = in use, do not evict
    is_dirty: AtomicBool,              // true = modified, must flush before evict
    last_access: AtomicU64,            // monotonic timestamp for LRU
}

/// Eviction policy trait — pluggable
pub trait EvictionPolicy: Send {
    fn on_access(&mut self, frame_idx: usize);
    fn choose_victim(&self) -> Option<usize>;  // only unpinned frames
}

/// CLOCK (Second Chance) — simple, effective for toy DB
pub struct ClockEvictor {
    hand: usize,
    reference_bits: Vec<bool>,
}

impl EvictionPolicy for ClockEvictor {
    fn on_access(&mut self, frame_idx: usize) {
        self.reference_bits[frame_idx] = true;
    }

    fn choose_victim(&self) -> Option<usize> {
        for _ in 0..self.reference_bits.len() {
            if !self.reference_bits[self.hand] {
                return Some(self.hand);
            }
            self.reference_bits[self.hand] = false;
            self.hand = (self.hand + 1) % self.reference_bits.len();
        }
        None  // all frames pinned or referenced
    }
}
```

**Buffer Pool API:**

```rust
impl BufferPool {
    /// Pin a page in memory. Blocks if all frames are occupied.
    /// Returns a guard that un-pins on drop.
    pub fn fix_page(&self, page_id: PageId) -> Result<BufferGuard> { /* ... */ }

    /// Flush all dirty pages to disk
    pub fn flush_all(&self) -> Result<()> { /* ... */ }
}

/// RAII guard — un-pins the page on Drop
pub struct BufferGuard<'a> {
    pool: &'a BufferPool,
    frame_idx: usize,
}

impl<'a> Drop for BufferGuard<'a> {
    fn drop(&mut self) {
        self.pool.frames[self.frame_idx].pin_count.fetch_sub(1, Ordering::Release);
    }
}

impl<'a> Deref for BufferGuard<'a> {
    type Target = RwLock<Page>;
    fn deref(&self) -> &Self::Target {
        &self.pool.frames[self.frame_idx].page
    }
}
```

### Disk Manager

```rust
pub trait DiskManager: Send + Sync {
    fn read_page(&self, page_id: PageId) -> Result<[u8; PAGE_SIZE]>;
    fn write_page(&self, page_id: PageId, data: &[u8; PAGE_SIZE]) -> Result<()>;
    fn allocate_page(&self) -> Result<PageId>;
    fn free_page(&self, page_id: PageId) -> Result<()>;
    fn sync(&self) -> Result<()>;  // fsync the database file
}

/// File-based implementation
pub struct FileDiskManager {
    file: Mutex<File>,
    freelist: Mutex<Vec<PageId>>,
    num_pages: AtomicU32,
}

impl DiskManager for FileDiskManager {
    fn read_page(&self, page_id: PageId) -> Result<[u8; PAGE_SIZE]> {
        let offset = page_id as u64 * PAGE_SIZE as u64;
        let mut buf = [0u8; PAGE_SIZE];
        self.file.lock().unwrap().read_exact_at(&mut buf, offset)?;
        // Verify checksum
        let stored_crc = u32::from_be_bytes(buf[12..16].try_into().unwrap());
        let computed = crc32(&buf[..12]);  // checksum header fields only
        if stored_crc != computed {
            return Err(DbError::ChecksumMismatch(page_id));
        }
        Ok(buf)
    }
    // ...
}
```

### Write-Ahead Log (WAL)

**Why WAL?** Without it, a crash during a multi-key write leaves the database in an inconsistent state (some keys updated, others not). WAL ensures atomicity: either all changes in a transaction are applied, or none are.

**Design decision: Logical WAL.** We log **operations**, not physical pages. A key update produces a small entry `(txn_id, key, old_value, new_value)` — typically tens of bytes — rather than a 4096-byte page snapshot. This keeps WAL compact, but recovery must replay operations on the live B+Tree rather than memcpy-ing pages.

**Format:**

```
WAL file layout:
┌──────────────────────────────────────────────────────────────┐
│ Header (32 bytes)                                             │
│  - magic: [u8; 4]        = b"WAL\0"                          │
│  - file_format: u32      = 1                                  │
│  - checkpoint_lsn: u64   = LSN of last checkpoint             │
│  - checksum: u32         = CRC32 of header                    │
├──────────────────────────────────────────────────────────────┤
│ Entry 0                                                       │
│  - lsn: u64              = monotonic log sequence number      │
│  - txn_id: u64           = transaction that wrote this        │
│  - op_type: u8           = PUT=0, DELETE=1, COMMIT=2          │
│  - key_len: u16                                               │
│  - key: [u8; key_len]                                         │
│  - value_len: u16        = 0 for DELETE/COMMIT                │
│  - [value: u8; value_len] = only for PUT                      │
│  - checksum: u32         = CRC32 of entry                     │
├──────────────────────────────────────────────────────────────┤
│ Entry 1 ... Entry N                                           │
│  - Last entry per txn: op_type=COMMIT (marks txn durable)     │
└──────────────────────────────────────────────────────────────┘
```

```rust
pub type LSN = u64;   // Log Sequence Number

#[repr(u8)]
enum WalOpType {
    Put = 0,     // key insert or update
    Delete = 1,  // key deletion (tombstone)
    Commit = 2,  // transaction commit marker
}

struct WalEntry {
    lsn: LSN,
    txn_id: u64,
    op_type: WalOpType,
    key: Vec<u8>,
    value: Option<Vec<u8>>,  // None for Delete and Commit
}

pub struct WalWriter {
    file: Mutex<File>,
    next_lsn: AtomicU64,           // monotonic LSN counter
    pending: Mutex<Vec<WalEntry>>, // uncommitted entries
    committed_lsn: AtomicU64,      // last committed LSN
}

impl WalWriter {
    /// Log a key modification. Does NOT fsync — caller batches, then calls commit().
    pub fn log_put(&self, txn_id: u64, key: &[u8], value: &[u8]) -> Result<LSN> {
        let lsn = self.next_lsn.fetch_add(1, Ordering::SeqCst);
        let mut pending = self.pending.lock().unwrap();
        pending.push(WalEntry { lsn, txn_id, op_type: WalOpType::Put, key: key.to_vec(), value: Some(value.to_vec()) });
        Ok(lsn)
    }

    pub fn log_delete(&self, txn_id: u64, key: &[u8]) -> Result<LSN> {
        let lsn = self.next_lsn.fetch_add(1, Ordering::SeqCst);
        let mut pending = self.pending.lock().unwrap();
        pending.push(WalEntry { lsn, txn_id, op_type: WalOpType::Delete, key: key.to_vec(), value: None });
        Ok(lsn)
    }

    /// Commit: append commit entry, fsync ALL pending entries. Durability guarantee.
    pub fn commit(&self, txn_id: u64) -> Result<()> {
        let lsn = self.next_lsn.fetch_add(1, Ordering::SeqCst);
        let mut pending = self.pending.lock().unwrap();
        pending.push(WalEntry { lsn, txn_id, op_type: WalOpType::Commit, key: vec![], value: None });

        let mut file = self.file.lock().unwrap();
        for entry in pending.iter() {
            write_entry(&mut file, entry)?;
        }
        file.sync_all()?;  // THIS is the durability guarantee

        self.committed_lsn.store(lsn, Ordering::SeqCst);
        pending.clear();
        Ok(())
    }

    /// Checkpoint: replay all committed entries into the main DB (B+Tree).
    /// After checkpoint, entries before checkpoint_lsn can be truncated.
    pub fn checkpoint(&self, btree: &BPlusTree) -> Result<()> {
        let checkpoint_lsn = self.committed_lsn.load(Ordering::SeqCst);
        let entries = self.read_entries_from(/* last_checkpoint_lsn */, checkpoint_lsn)?;
        for entry in entries {
            match entry.op_type {
                WalOpType::Put => btree.insert(&entry.key, entry.value.as_ref().unwrap())?,
                WalOpType::Delete => btree.delete(&entry.key)?,
                WalOpType::Commit => { /* no-op — entries already applied */ }
            }
        }
        self.update_checkpoint_header(checkpoint_lsn)?;
        Ok(())
    }

    /// Crash recovery: replay all committed entries since last checkpoint.
    pub fn recover(&self, btree: &mut BPlusTree) -> Result<()> {
        let checkpoint_lsn = self.read_checkpoint_lsn()?;
        let entries = self.read_entries_from(checkpoint_lsn, u64::MAX)?;

        // Group by txn_id, find committed txns (those with a Commit entry)
        let mut txn_entries: HashMap<u64, Vec<&WalEntry>> = HashMap::new();
        for entry in &entries {
            txn_entries.entry(entry.txn_id).or_default().push(entry);
        }

        // Replay only committed transactions in LSN order
        for entry in entries {
            if entry.op_type == WalOpType::Commit {
                // Replay this txn's entries (they're before the Commit in WAL order)
                for txn_entry in txn_entries.get(&entry.txn_id).unwrap_or(&vec![]) {
                    match txn_entry.op_type {
                        WalOpType::Put => btree.insert(&txn_entry.key, txn_entry.value.as_ref().unwrap())?,
                        WalOpType::Delete => btree.delete(&txn_entry.key)?,
                        _ => {}
                    }
                }
            }
        }
        // Truncate WAL after last committed entry
        self.truncate_after(checkpoint_lsn)?;
        Ok(())
    }
}
```

### Integration: B+Tree → BufferPool → DiskManager → WAL

```rust
impl PageStore for BufferPool {
    fn read_page(&self, id: PageId) -> Result<Page> {
        // 1. Check if page is already in buffer pool
        // 2. If not: read from DiskManager, insert into pool (may evict)
        // 3. Pin page, return BufferGuard
    }

    fn write_page(&self, page: &Page) -> Result<()> {
        // 1. If WAL enabled: log before-image + after-image to WAL
        // 2. Mark buffer frame as dirty
        // 3. On checkpoint or eviction: flush to disk
    }
}
```

### Production Pattern Comparison

| Concept | SQLite | PostgreSQL | Our Toy DB |
|---------|--------|-----------|------------|
| Page size | 512–65536 (default 4096) | 8192 (default) | 4096 (fixed) |
| B-Tree variant | B-Tree (keys removed from internal nodes) | B-Tree (Lehman-Yao for concurrency) | B+Tree (keys copied up, leaf chain) |
| Buffer eviction | LRU with list | Clock sweep + usage count | Clock (second chance) |
| WAL checksum | Per-frame, with salt | Full-page images + WAL | Per-frame CRC32 |
| Free list | Trunk+leaf pages | Free Space Map (FSM) | Simple linked list |

---

## Phase 3: Concurrency — Latch Coupling

### Goal

Multiple threads can read the B+Tree simultaneously. Writers block only overlapping operations, not the entire tree.

### Success Criteria
- [ ] 4 concurrent readers scan different leaves simultaneously (zero contention)
- [ ] Writer blocks readers only on overlapping nodes, not globally
- [ ] Optimistic insert succeeds >90% of the time (no restart needed)
- [ ] No deadlocks under concurrent workload
- [ ] Stress test: 8 readers + 2 writers, 100K operations, no data corruption

### Page-Level Latch

```rust
/// Latch mode for page access
pub enum LatchMode {
    Shared,      // multiple readers can hold simultaneously
    Exclusive,   // only one writer, blocks all others
}

/// Trait for page-level latching — pluggable for testing
pub trait PageLatch: Send + Sync {
    fn latch(&self, mode: LatchMode);
    fn unlatch(&self);
    fn try_upgrade(&self) -> bool;  // shared → exclusive without releasing
}

/// Toy implementation using std::sync::RwLock
pub struct RwLockLatch {
    inner: RwLock<()>,
}

impl PageLatch for RwLockLatch {
    fn latch(&self, mode: LatchMode) {
        match mode {
            LatchMode::Shared    => { let _ = self.inner.read().unwrap(); }
            LatchMode::Exclusive => { let _ = self.inner.write().unwrap(); }
        }
    }

    fn unlatch(&self) {
        // RwLock guard is dropped, releasing the lock
    }

    fn try_upgrade(&self) -> bool {
        // RwLock doesn't support upgrade natively.
        // Toy DB workaround: drop shared, acquire exclusive (non-atomic).
        // Production: use a custom latch that supports upgrade.
        false
    }
}
```

Each `BufferFrame` holds a `PageLatch`. Before accessing page data, the thread acquires the latch. The latch is released when the `BufferGuard` is dropped.

### Latch Coupling Protocol (Bayer-Schkolnick)

**The golden rule: ALWAYS acquire latches top-down (root → leaf).** Never acquire a parent latch after acquiring a child latch. This prevents deadlock.

**Read traversal (Shared latches, coupling):**

```rust
fn traverse_read(&self, key: &[u8]) -> Result<Option<Value>> {
    let mut guard = self.buffer_pool.fix_page(self.root_page_id)?;
    guard.latch(LatchMode::Shared);

    loop {
        if guard.page_type() == PageType::Leaf {
            let result = guard.search(key);
            guard.unlatch();
            return Ok(result);
        }

        // Inner node — find child
        let child_idx = guard.lower_bound(key);
        let child_id = guard.child_at(child_idx);

        // Acquire child latch BEFORE releasing parent (coupling)
        let child_guard = self.buffer_pool.fix_page(child_id)?;
        child_guard.latch(LatchMode::Shared);

        // Release parent — child is now latched, safe to traverse
        guard.unlatch();
        guard = child_guard;
    }
}
```

**Optimistic write traversal (Shared down, upgrade at leaf):**

```rust
fn traverse_write_optimistic(&self, key: &[u8], value: &[u8]) -> Result<()> {
    // Phase 1: Share-latched descent (same as read)
    let mut path: Vec<BufferGuard> = Vec::new();

    let mut guard = self.buffer_pool.fix_page(self.root_page_id)?;
    guard.latch(LatchMode::Shared);
    path.push(guard);

    // ... traverse down with shared latches, building path ...

    // Phase 2: At leaf — upgrade to exclusive
    let leaf_guard = path.last_mut().unwrap();
    if !leaf_guard.try_upgrade() {
        // Can't upgrade atomically — release ALL, restart with exclusive from root
        for g in path.iter_mut().rev() { g.unlatch(); }
        return self.traverse_write_pessimistic(key, value);
    }

    // Phase 3: Check if leaf is safe (won't split)
    if leaf_guard.has_space(self.order) {
        leaf_guard.insert_cell(key, value);
        leaf_guard.mark_dirty();
        // Unlatch everything
        for g in path.iter_mut().rev() { g.unlatch(); }
        return Ok(());
    }

    // Phase 4: Unsafe — leaf will split. Release all, restart pessimistic.
    for g in path.iter_mut().rev() { g.unlatch(); }
    self.traverse_write_pessimistic(key, value)
}
```

**Pessimistic write traversal (Exclusive on unsafe nodes):**

When the optimistic path fails (leaf would split), restart from root, acquiring exclusive latches on any node that might be involved in the split cascade.

```rust
fn traverse_write_pessimistic(&self, key: &[u8], value: &[u8]) -> Result<()> {
    // Acquire exclusive latches top-down. Keep parent latched until child is
    // known to be safe (won't split).
    // If a node is at capacity (unsafe for insert), keep its parent latched.
    // This ensures no other writer can modify ancestors during split propagation.
}
```

**Safe node definition:**
- For INSERT: node has room for ≥1 more cell (`num_cells < order`)
- For DELETE: node has >⌈order/2⌉ cells (won't underflow below minimum)

### Performance Characteristics

| Operation | Shared Latch Holders | Exclusive Wait | Restart Rate |
|-----------|---------------------|-----------------|-------------|
| Read | Leaf only (brief) | Never blocked by reads | 0% |
| Optimistic Insert | Path nodes (released as we go) | Only if leaf splits (~1%) | <5% at high load |
| Pessimistic Insert | Root→leaf (held until done) | Blocks all on same branch | 0% (by design) |

---

## Phase 4: MVCC Transactions

### Goal

Concurrent transactions see consistent snapshots of the database without blocking each other. Committed data survives crashes; uncommitted data is invisible.

### Success Criteria
- [ ] Two concurrent transactions see their own writes, not each other's uncommitted writes
- [ ] Committed transaction data visible to all subsequent transactions
- [ ] Rollback discards all writes, database returns to pre-transaction state
- [ ] Snapshot isolation: readers never blocked by writers, writers never blocked by readers
- [ ] Garbage collection reduces version chains to last 2 versions

### Design (PostgreSQL-inspired Snapshot Isolation with First-Committer-Wins)

Each key has a **version chain** — a linked list of historical values, newest first. Each version is tagged with the transaction ID that created it.

**Conflict resolution: First-committer-wins (optimistic).** Two concurrent transactions can both write the same key — MVCC creates two versions. At commit time, the second committer checks if any key in its write-set was modified by a transaction that committed after its snapshot was taken. If so → write-write conflict → second committer must abort and retry. This is the same model as PostgreSQL's Repeatable Read.

**Hybrid version cache.** The B+Tree value stores the last 2 versions inline for fast current-state and recent-history access. Older versions live only in the WAL, reachable via `prev_lsn` pointers. GC prunes WAL entries when no active snapshot needs them.

```rust
/// Monotonically increasing transaction identifier
pub type TxnId = u64;

/// A version of a key's value
struct Version {
    txn_id: TxnId,                 // transaction that created this version
    value: Option<Vec<u8>>,        // None = tombstone (deleted)
    prev_lsn: Option<LSN>,         // pointer to previous version in WAL
    created_at: Instant,           // for GC age-based eviction
}

/// Ordered newest-first (Vec index 0 = latest version)
struct VersionChain {
    versions: Vec<Version>,
}
```

```rust
/// The MVCC engine wraps the B+Tree and adds transaction support
pub struct MvccEngine {
    btree: Arc<BPlusTree>,                  // stores (key → VersionChain)
    next_txn_id: AtomicU64,
    active_txns: RwLock<BTreeSet<TxnId>>,   // currently running transactions
    committed_txns: RwLock<BTreeMap<TxnId, LSN>>,  // committed txn → commit LSN
    wal: Arc<WalWriter>,                    // WAL serves as commit log for MVCC
}

/// A snapshot represents the database state visible to a transaction
struct Snapshot {
    xmin: TxnId,              // oldest active txn when snapshot was taken
    xmax: TxnId,              // next txn ID at snapshot time (exclusive bound)
    active_list: Vec<TxnId>,  // all active txns at snapshot time
}

impl MvccEngine {
    /// Start a new transaction. Assigns a monotonically increasing TxnId
    /// and FREEZES a snapshot at this moment. All subsequent reads in this
    /// transaction see data as it existed at begin() — Repeatable Read.
    /// Write-only transactions (no reads before writes) still get a snapshot
    /// for conflict detection at commit time.
    pub fn begin(&self) -> Result<TxnId> {
        let txn_id = self.next_txn_id.fetch_add(1, Ordering::SeqCst);
        let snapshot = self.capture_snapshot(txn_id);
        self.active_txns.write().unwrap().insert(txn_id);
        self.snapshots.write().unwrap().insert(txn_id, snapshot);
        Ok(txn_id)
    }

    /// Capture the database snapshot at the moment begin() is called
    fn get_snapshot(&self, txn_id: TxnId) -> Snapshot {
        let active = self.active_txns.read().unwrap();
        Snapshot {
            xmin: active.first().copied().unwrap_or(txn_id),
            xmax: txn_id,
            active_list: active.iter().copied().collect(),
        }
    }

    /// Read a key, returning the version visible to this transaction
    pub fn read(&self, txn_id: TxnId, key: &[u8]) -> Result<Option<Value>> {
        let chain = self.btree.lookup(key)?;
        let snapshot = self.get_snapshot(txn_id);

        // Walk version chain (newest first), find first visible version:
        //   1. Our own writes are always visible (txn_id matches)
        //   2. Committed by someone else AND txn_id < snapshot.xmin → visible
        //   3. Committed by someone else BUT txn_id in snapshot.active_list → invisible
        //   4. Uncommitted by someone else → invisible

        for version in &chain.versions {
            if version.txn_id == txn_id {
                return Ok(version.value.clone());  // own writes
            }
            if version.txn_id < snapshot.xmin {
                return Ok(version.value.clone());  // committed before snapshot
            }
            if !snapshot.active_list.contains(&version.txn_id) {
                // Committed after snapshot was taken → invisible to us
                continue;
            }
        }
        Ok(None)
    }

    /// Write a key-value. Creates a new version, visible only to this txn.
    pub fn write(&self, txn_id: TxnId, key: &[u8], value: &[u8]) -> Result<()> {
        let mut chain = self.btree.lookup(key)?.unwrap_or(VersionChain::new());
        chain.versions.insert(0, Version {
            txn_id,
            value: Some(value.to_vec()),
            prev_lsn: None,  // will be set on commit
            created_at: Instant::now(),
        });
        self.btree.insert(key, chain)?;
        Ok(())
    }

    /// Commit: make all writes durable and visible. First-committer-wins:
    /// if any key in our write-set was modified by a txn that committed
    /// after our snapshot was taken → conflict → abort.
    pub fn commit(&self, txn_id: TxnId, write_set: &WriteSet) -> Result<()> {
        let snapshot = self.get_snapshot(txn_id);

        // Conflict detection: check if any key we wrote was also
        // written by a transaction that committed after our snapshot.
        for key in &write_set.keys {
            let chain = self.btree.lookup(key)?;
            if let Some(chain) = chain {
                if let Some(newest) = chain.inline_versions.first() {
                    if newest.txn_id != txn_id
                        && newest.txn_id >= snapshot.xmin
                        && !snapshot.active_list.contains(&newest.txn_id)
                    {
                        // Someone else committed a write to this key
                        // after our snapshot — write-write conflict.
                        return Err(DbError::WriteConflict);
                    }
                }
            }
        }

        // 1. Write commit entry to WAL
        self.wal.commit(txn_id)?;

        // 2. Mark txn as committed, remove from active
        self.active_txns.write().unwrap().remove(&txn_id);
        self.committed_txns.write().unwrap().insert(txn_id, self.wal.current_lsn());
        Ok(())
    }

    /// Rollback: discard all versions created by this transaction
    pub fn rollback(&self, txn_id: TxnId) -> Result<()> {
        // For each key modified by this txn:
        //   1. Remove the version with txn_id from the chain
        //   2. If chain is now empty, delete the key from B+Tree
        self.active_txns.write().unwrap().remove(&txn_id);
        Ok(())
    }
}
```

### Garbage Collection

Old versions accumulate over time. Tomstones (`value = None`) occupy space in leaf nodes and slow down range scans. GC removes versions and tombstones no longer visible to any active or future transaction.

**Two-phase cleanup:**
1. **WAL checkpoint must run first** — ensures the latest committed state is durable in the main DB file. GC must never remove data that hasn't been checkpointed (otherwise crash recovery can't reconstruct state).
2. **GC runs after checkpoint** — for each key:
   - Remove inline versions where `txn_id < oldest_active_txn`
   - If the newest remaining version is a tombstone AND no active txn can see any previous version → **physically delete the key from the B+Tree** (free the leaf cell)
   - For versions exiled to WAL, remove WAL entries where `lsn < checkpoint_lsn` AND `txn_id < oldest_active_txn`

```rust
impl MvccEngine {
    pub fn vacuum(&self) -> Result<usize> {
        let checkpoint_lsn = self.wal.checkpoint_lsn();
        let active = self.active_txns.read().unwrap();
        let oldest_active = active.first().copied().unwrap_or(u64::MAX);

        let mut removed = 0;
        // Iterate B+Tree, for each key:
        //   1. Prune inline_versions: keep only versions where txn_id >= oldest_active
        //   2. Keep at least 1 version (current state)
        //   3. If version is tombstone AND invisible to all → delete key from B+Tree
        //   4. Prune WAL entries with lsn < checkpoint_lsn AND txn_id < oldest_active
        Ok(removed)
    }
}
```

### Transaction Isolation Levels

| Level | Supported | Implementation |
|-------|-----------|----------------|
| Read Uncommitted | No | Not useful for a toy DB — skip |
| Read Committed | Yes | Take snapshot at statement start |
| Repeatable Read | Yes | Take snapshot at transaction start (**default**) |
| Serializable | No | Requires SSI (Serializable Snapshot Isolation) — too complex for toy |

---

## Phase 5: SQL Frontend

### Goal

Accept SQL text input, parse it, plan execution, and run against the MVCC engine.

### Success Criteria
- [ ] `CREATE TABLE`, `INSERT`, `SELECT` with `WHERE` clause all work end-to-end
- [ ] Query plan printed for debugging (explain-like output)
- [ ] Basic type checking: reject inserting string into INT column
- [ ] Error messages include line/column numbers

### Architecture: Classic Parser → Planner → Executor

```
SQL text → Lexer → Tokens → Parser → AST → Planner → Plan Tree → Executor → MVCC Engine
```

### Lexer: Text → Token Stream

```rust
#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    // Keywords
    Select, From, Where, Insert, Into, Values,
    Create, Table, Int, Text, Primary, Key, Delete,

    // Literals
    Ident(String),
    Number(i64),
    StringLit(String),

    // Symbols
    Eq, Neq, Lt, Gt, Le, Ge,           // = != < > <= >=
    Comma, LParen, RParen, Semicolon,
    Asterisk,                            // *

    // End of input
    Eof,
}

pub struct Lexer {
    input: Vec<char>,
    pos: usize,
}

impl Lexer {
    pub fn tokenize(input: &str) -> Result<Vec<Token>> { /* ... */ }

    fn next_token(&mut self) -> Result<Token> {
        self.skip_whitespace();
        match self.current() {
            Some('=') => { self.advance(); Ok(Token::Eq) }
            Some('\'') => self.lex_string(),
            Some(c) if c.is_alphabetic() => self.lex_ident_or_keyword(),
            Some(c) if c.is_ascii_digit() => self.lex_number(),
            // ...
        }
    }
}
```

### Parser: Tokens → AST (Recursive Descent)

```rust
#[derive(Debug)]
pub enum Statement {
    Select {
        columns: Vec<SelectColumn>,
        table: String,
        where_clause: Option<Expr>,
    },
    Insert {
        table: String,
        columns: Vec<String>,
        values: Vec<Expr>,
    },
    CreateTable {
        name: String,
        columns: Vec<ColumnDef>,
    },
    Delete {
        table: String,
        where_clause: Option<Expr>,
    },
}

pub enum SelectColumn {
    All,                     // SELECT *
    Named(String),           // SELECT name
}

pub enum Expr {
    Column(String),
    Literal(Value),
    BinOp(Box<Expr>, BinOp, Box<Expr>),
}

pub enum BinOp {
    Eq, Neq, Lt, Gt, Le, Ge,
    And, Or,
}

pub struct ColumnDef {
    pub name: String,
    pub data_type: DataType,
    pub is_primary_key: bool,
}

pub enum DataType { Int, Text }

pub struct Parser {
    tokens: Vec<Token>,
    pos: usize,
}

impl Parser {
    pub fn parse(&mut self) -> Result<Vec<Statement>> {
        let mut stmts = Vec::new();
        while self.current() != Some(&Token::Eof) {
            stmts.push(self.parse_statement()?);
            self.expect(Token::Semicolon)?;
        }
        Ok(stmts)
    }

    fn parse_statement(&mut self) -> Result<Statement> {
        match self.current() {
            Some(Token::Select) => self.parse_select(),
            Some(Token::Insert) => self.parse_insert(),
            Some(Token::Create) => self.parse_create_table(),
            Some(Token::Delete) => self.parse_delete(),
            _ => Err(DbError::ParseError("expected SELECT, INSERT, CREATE, or DELETE".into())),
        }
    }

    fn parse_select(&mut self) -> Result<Statement> {
        self.expect(Token::Select)?;
        let columns = self.parse_select_columns()?;
        self.expect(Token::From)?;
        let table = self.expect_ident()?;
        let where_clause = if self.matches(Token::Where) {
            self.advance();
            Some(self.parse_expr()?)
        } else { None };
        Ok(Statement::Select { columns, table, where_clause })
    }

    // INSERT INTO table (col1, col2) VALUES (val1, val2);
    fn parse_insert(&mut self) -> Result<Statement> { /* ... */ }

    // CREATE TABLE name (col1 INT PRIMARY KEY, col2 TEXT);
    fn parse_create_table(&mut self) -> Result<Statement> { /* ... */ }

    // DELETE FROM table WHERE condition;
    fn parse_delete(&mut self) -> Result<Statement> { /* ... */ }
}
```

### Planner: AST → Query Plan Tree

```rust
pub enum PlanNode {
    /// Full table scan (no index available)
    SeqScan {
        table: String,
        filter: Option<Expr>,
        columns: Vec<SelectColumn>,
    },

    /// Index range scan (when WHERE filters on primary key)
    IndexScan {
        table: String,
        index_name: String,
        range_start: Option<Value>,
        range_end: Option<Value>,
        columns: Vec<SelectColumn>,
    },

    /// Insert a row into a table
    InsertRow {
        table: String,
        values: Vec<(String, Value)>,   // (column_name, value)
    },

    /// Create a new table (system catalog operation)
    CreateTable {
        name: String,
        columns: Vec<ColumnDef>,
    },

    /// Delete rows matching filter
    DeleteRows {
        table: String,
        filter: Option<Expr>,
    },
}

impl Planner {
    pub fn plan(&self, stmt: &Statement) -> Result<PlanNode> {
        match stmt {
            Statement::Select { columns, table, where_clause } => {
                // If WHERE clause filters on primary key with equality,
                // use IndexScan. Otherwise, SeqScan.
                if let Some(Expr::BinOp(box Expr::Column(col), BinOp::Eq, box Expr::Literal(val)))
                    = where_clause
                {
                    if col == "id" {  // primary key
                        return Ok(PlanNode::IndexScan {
                            table: table.clone(),
                            index_name: format!("{}_pkey", table),
                            range_start: Some(val.clone()),
                            range_end: Some(val.clone()),
                            columns: columns.clone(),
                        });
                    }
                }
                Ok(PlanNode::SeqScan {
                    table: table.clone(),
                    filter: where_clause.clone(),
                    columns: columns.clone(),
                })
            }
            Statement::Insert { table, columns, values } => {
                let pairs: Vec<_> = columns.iter()
                    .zip(values.iter())
                    .map(|(c, v)| (c.clone(), v.eval_to_value()))
                    .collect();
                Ok(PlanNode::InsertRow { table: table.clone(), values: pairs })
            }
            // ... CreateTable, DeleteRows ...
        }
    }
}
```

### Executor: Plan Tree → MVCC Operations

```rust
pub struct Executor {
    engine: Arc<MvccEngine>,
    catalog: Catalog,  // table schemas
}

impl Executor {
    pub fn execute(&self, plan: &PlanNode, txn_id: TxnId) -> Result<QueryResult> {
        match plan {
            PlanNode::SeqScan { table, filter, columns } => {
                self.exec_seq_scan(table, filter, columns, txn_id)
            }
            PlanNode::IndexScan { table, range_start, range_end, columns } => {
                self.exec_index_scan(table, range_start, range_end, columns, txn_id)
            }
            PlanNode::InsertRow { table, values } => {
                self.exec_insert(table, values, txn_id)
            }
            PlanNode::CreateTable { name, columns } => {
                self.exec_create_table(name, columns, txn_id)
            }
            PlanNode::DeleteRows { table, filter } => {
                self.exec_delete(table, filter, txn_id)
            }
        }
    }
}

pub enum QueryResult {
    Rows(Vec<Vec<Value>>),       // SELECT result
    RowsAffected(usize),         // INSERT/DELETE count
    TableCreated,                // CREATE TABLE success
    Explain(String),             // EXPLAIN output
}
```

### SQL Subset Supported

```sql
-- DDL
CREATE TABLE users (
    id INT PRIMARY KEY,
    name TEXT,
    age INT
);

-- DML
INSERT INTO users (id, name, age) VALUES (1, 'Alice', 30);
INSERT INTO users VALUES (2, 'Bob', 25);

-- Query
SELECT * FROM users;
SELECT id, name FROM users WHERE age > 25;
SELECT * FROM users WHERE id = 1;

-- Delete
DELETE FROM users WHERE id = 2;
```

**Intentionally excluded from Phase 5 (saved for Phase 5b):**
- JOINs (requires nested loop or hash join)
- Subqueries
- Aggregations (`GROUP BY`, `COUNT`, `SUM`)
- `ORDER BY`
- `UPDATE` statement
- `NULL` handling

### System Catalog and Table Key Namespacing

**All tables share a single B+Tree instance.** Keys are namespaced by a 2-byte `table_id` prefix:

```
B+Tree key format: [table_id: u16][primary_key_bytes: N]

Example:
  table_id=1 (users):   [0x00, 0x01, 0x00, 0x00, 0x00, 0x01]  → user id=1
  table_id=2 (orders):  [0x00, 0x02, 0x00, 0x00, 0x00, 0x01]  → order id=1
```

This is the same approach SQLite takes internally. Benefits:
- Range scan on one table = range scan on `[table_id][0x00...]` to `[table_id][0xFF...]` — naturally contiguous
- The system catalog is table_id=0 (reserved)
- No separate B+Tree instance per table needed

```rust
struct Catalog {
    // Internal tables:
    //   "__catalog_tables":   table_name → (table_id,)
    //   "__catalog_columns":  (table_id, column_index) → (name, type, is_pk)
    engine: Arc<MvccEngine>,
}
```

---

## Phase 6: Distribution — Raft Replication

### Goal

A 3-node cluster tolerates 1 node failure. Committed data survives leader crashes. New leader elected automatically.

### Success Criteria
- [ ] 3-node cluster survives 1 node failure (majority available)
- [ ] New leader elected within 5 seconds of leader crash
- [ ] Committed entries survive leader crash (no lost data)
- [ ] Read-your-writes: leader reads reflect all committed entries
- [ ] `kubectl delete pod` and cluster self-heals

### Raft Basics (Simplified)

Raft replicates a log across nodes. Our WAL serves as the Raft log — each WAL entry is a Raft log entry. This elegantly unifies crash recovery and replication.

```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum RaftRole {
    Follower,
    Candidate,
    Leader,
}

pub struct RaftNode {
    id: NodeId,

    // Persistent state (survives restart)
    current_term: AtomicU64,
    voted_for: RwLock<Option<NodeId>>,
    log: Arc<WalWriter>,          // WAL = Raft log. Unified!

    // Volatile state
    role: RwLock<RaftRole>,
    commit_index: AtomicU64,
    last_applied: AtomicU64,

    // Leader state
    next_index: RwLock<HashMap<NodeId, u64>>,
    match_index: RwLock<HashMap<NodeId, u64>>,

    // Cluster config
    peers: Vec<NodeId>,
    rpc: Arc<dyn RaftRpc>,
}

pub type NodeId = u64;

/// RPC interface for inter-node communication
pub trait RaftRpc: Send + Sync {
    fn request_vote(&self, term: u64, candidate_id: NodeId,
                    last_log_index: u64, last_log_term: u64)
        -> Result<(u64, bool)>;  // (term, vote_granted)

    fn append_entries(&self, term: u64, leader_id: NodeId,
                      prev_log_index: u64, prev_log_term: u64,
                      entries: Vec<LogEntry>, leader_commit: u64)
        -> Result<(u64, bool)>;  // (term, success)
}
```

### Leader Election

```
1. Follower times out (randomized 150-300ms election timeout)
2. Becomes Candidate, increments term, votes for self
3. Sends RequestVote RPC to all peers
4. If majority votes yes → become Leader
5. If receive AppendEntries from higher term → revert to Follower
6. If election timeout elapses without majority → increment term, restart election
```

### Log Replication

```
Leader receives client write:
1. Append entry to local WAL (uncommitted)
2. Send AppendEntries to all followers (with new entry)
3. On majority ACK → mark entry as committed (advance commit_index)
4. Apply committed entry to state machine (MVCC engine)
5. Return success to client
```

### Raft + MVCC Integration

**Determinism guarantee: log both old_val + new_val.** When the leader writes `(txn_id, key, old_val, new_val)` to the WAL, and replicates this to followers, the follower applies by DIRECTLY setting the key to `new_val` — it does NOT re-execute `mvcc_engine.write()` which would compute `old_val` locally. This ensures the follower's state is byte-for-byte identical to the leader's, regardless of any divergence in transaction history during catch-up.

```rust
/// Raft log entry — contains everything needed for deterministic replay
struct RaftLogEntry {
    term: u64,
    index: u64,
    command: LogCommand,
}

enum LogCommand {
    /// Key modification — old_val for verification, new_val for application
    Put { txn_id: u64, key: Vec<u8>, old_value: Vec<u8>, new_value: Vec<u8> },
    Delete { txn_id: u64, key: Vec<u8>, old_value: Vec<u8> },
    CommitTxn { txn_id: u64 },
    RollbackTxn { txn_id: u64 },
}

impl RaftNode {
    /// Apply a committed log entry to the state machine.
    /// Followers apply SAME entries as the leader — deterministic replay.
    fn apply_entry(&self, entry: &RaftLogEntry) -> Result<()> {
        match &entry.command {
            LogCommand::Put { txn_id, key, old_value: _, new_value } => {
                // Directly set — don't re-derive old_value locally
                self.mvcc_engine.apply_put(*txn_id, key, new_value)?;
            }
            LogCommand::Delete { txn_id, key, old_value: _ } => {
                self.mvcc_engine.apply_delete(*txn_id, key)?;
            }
            LogCommand::CommitTxn { txn_id } => {
                self.mvcc_engine.commit(*txn_id, /* write_set from Put entries */)?;
            }
            LogCommand::RollbackTxn { txn_id } => {
                self.mvcc_engine.rollback(*txn_id)?;
            }
        }
        Ok(())
    }
}
```

**Key insight:** The WAL is shared between crash recovery and Raft replication. A single durable log serves both purposes — the same pattern used by etcd and CockroachDB. Raft log entries are WAL entries; Raft commit is WAL fsync. Followers replay WAL entries deterministically using `old_value` verification.

---

## Rust Crate Structure

```
toydb/
├── Cargo.toml
├── src/
│   ├── main.rs              # CLI entry point (REPL or server)
│   ├── lib.rs               # Public API re-exports
│   ├── error.rs             # DbError enum, Result alias
│   │
│   ├── storage/             # Phase 1-2: Core storage engine
│   │   ├── mod.rs
│   │   ├── page.rs          # Page, PageHeader, PageType, serialization
│   │   ├── btree.rs         # BPlusTree: lookup, insert, delete, range_scan
│   │   ├── buffer.rs        # BufferPool, BufferFrame, BufferGuard, ClockEvictor
│   │   ├── disk.rs          # DiskManager trait, FileDiskManager
│   │   └── wal.rs           # WalWriter: log_write, commit, checkpoint, recover
│   │
│   ├── concurrency/         # Phase 3: Thread safety
│   │   ├── mod.rs
│   │   └── latch.rs         # PageLatch trait, RwLockLatch, latch coupling
│   │
│   ├── mvcc/                # Phase 4: Transactions
│   │   ├── mod.rs
│   │   ├── engine.rs        # MvccEngine: begin, read, write, commit, rollback
│   │   ├── snapshot.rs      # Snapshot, visibility checking
│   │   ├── version.rs       # Version, VersionChain
│   │   └── gc.rs            # vacuum: prune old versions
│   │
│   ├── sql/                 # Phase 5: SQL frontend
│   │   ├── mod.rs
│   │   ├── lexer.rs         # tokenize: text → tokens
│   │   ├── parser.rs        # parse: tokens → AST (Statements)
│   │   ├── planner.rs       # plan: AST → PlanNode tree
│   │   ├── executor.rs      # execute: PlanNode → MVCC operations
│   │   └── catalog.rs       # system catalog (table/column metadata)
│   │
│   └── raft/                # Phase 6: Distribution
│       ├── mod.rs
│       ├── node.rs           # RaftNode: leader election, log replication
│       ├── rpc.rs            # RaftRpc trait, gRPC/HTTP implementation
│       └── state_machine.rs  # Apply committed entries to MVCC engine
│
└── tests/
    ├── btree_tests.rs        # Phase 1 tests
    ├── wal_tests.rs          # Phase 2 tests
    ├── concurrency_tests.rs  # Phase 3 tests
    ├── mvcc_tests.rs         # Phase 4 tests
    ├── sql_tests.rs          # Phase 5 tests
    └── raft_tests.rs         # Phase 6 tests
```

**Key Rust dependencies (Cargo.toml):**
```toml
[dependencies]
tokio = { version = "1", features = ["full"] }     # async runtime (Phase 6)
thiserror = "1"                                      # ergonomic error types
crc32fast = "1"                                      # page/WAL checksums
serde = { version = "1", features = ["derive"] }    # serialization (optional)
bytes = "1"                                          # zero-copy byte buffers

[dev-dependencies]
rand = "0.8"        # random test data
tempfile = "3"      # temp DB files for tests
proptest = "1"      # property-based testing (optional)
```

---

## Development Roadmap

### Phase 1: In-Memory B+Tree (Week 1-2)
- [ ] Page layout + serialization (`page.rs`)
- [ ] `MemoryPageStore` for testing
- [ ] `BPlusTree::lookup`, `insert`, `delete`, `range_scan`
- [ ] Leaf split, inner split, root split
- [ ] Test suite: insert/lookup/delete/range/invariants

### Phase 2: Persistence (Week 3-4)
- [ ] `FileDiskManager` with checksums
- [ ] `BufferPool` with Clock eviction
- [ ] `WalWriter`: log_write, commit, checkpoint, recover
- [ ] Integration: B+Tree backed by BufferPool
- [ ] Crash recovery test (kill process, restart, verify)

### Phase 3: Concurrency (Week 5-6)
- [ ] `PageLatch` trait + `RwLockLatch` implementation
- [ ] Latch coupling: read traversal, optimistic write, pessimistic write
- [ ] Safe node detection, restart on unsafe
- [ ] Stress tests: multi-threaded readers + writers

### Phase 4: MVCC (Week 7-8)
- [ ] `Version`, `VersionChain`, `Snapshot` data structures
- [ ] `MvccEngine`: begin, read (visibility), write, commit, rollback
- [ ] WAL as commit log integration
- [ ] Garbage collection (`vacuum`)
- [ ] Concurrent transaction tests

### Phase 5: SQL (Week 9-10)
- [ ] Lexer + Parser (recursive descent)
- [ ] Planner (AST → PlanNode)
- [ ] Executor (PlanNode → MVCC operations)
- [ ] System catalog
- [ ] End-to-end SQL tests

### Phase 6: Raft (Week 11-12)
- [ ] `RaftNode`: leader election, log replication
- [ ] RPC layer (gRPC or HTTP)
- [ ] Integration with WAL (shared log)
- [ ] Cluster test: 3 nodes, kill leader, verify recovery

---

## Success Criteria (All Phases)

1. **B+Tree Correctness:** 100K random inserts → all retrievable; delete 20% → rest intact; range scans correct across leaf boundaries; order derived from page layout (not magic constant)
2. **Persistence:** Kill -9 mid-write → restart → last committed state restored via logical WAL replay
3. **Concurrency:** 4 readers + 2 writers → zero deadlocks, zero data corruption; optimistic insert restart rate <5%
4. **MVCC Isolation:** Two concurrent txn see consistent snapshots frozen at begin(); rollback discards all writes; first-committer-wins correctly detects write-write conflicts
5. **MVCC GC:** Tombstones physically removed from B+Tree after checkpoint + no active txn can see them; version chains capped at 2 inline versions
6. **SQL:** `CREATE TABLE`, `INSERT`, `SELECT` with `WHERE` all work end-to-end; type errors caught; table_id namespace prefixing transparent to user
7. **Distribution:** 3-node cluster tolerates 1-node failure; new leader within 5s; follower state byte-identical to leader after log replay
8. **Buffer Hit Rate:** >80% on repeated lookups of same key range
9. **Write Conflict:** Two txns updating same key → second committer gets WriteConflict error and can retry
