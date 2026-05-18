# Toy Database Phase 1: In-Memory B+Tree — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A working single-threaded, in-memory B+Tree that supports insert, lookup, delete, and range scan on arbitrary byte keys/values.

**Architecture:** Fixed-size 4096-byte pages with a 16-byte header. Inner nodes store `(key, child_page_id)` cells. Leaf nodes store `(key, value)` cells and link via `next_leaf` for range scans. Node capacity is derived from page layout constants. A `MemoryPageStore` provides in-memory page allocation for testing.

**Tech Stack:** Rust (latest stable), `thiserror` for error types, `crc32fast` for page checksums, `rand` for test data.

---

## File Structure

```
toydb/
├── Cargo.toml
└── src/
    ├── lib.rs              # re-exports, Result alias
    ├── error.rs            # DbError enum
    ├── storage/
    │   ├── mod.rs
    │   ├── page.rs         # Page, PageHeader, PageType, cell layout
    │   ├── btree.rs        # BPlusTree: lookup, insert, delete, range_scan
    │   └── mem_store.rs    # MemoryPageStore for testing
    └── main.rs             # (empty, placeholder)
```

---

## Task 1: Project Setup + Error Types

**Files:**
- Create: `toydb/Cargo.toml`
- Create: `toydb/src/lib.rs`
- Create: `toydb/src/error.rs`
- Create: `toydb/src/main.rs`

- [ ] **Step 1: Create Cargo project and add dependencies**

```bash
cd /workspace/toydb  # or wherever the workspace is
cargo init --lib toydb
rm toydb/src/lib.rs toydb/src/main.rs  # rewrite from scratch
mkdir -p toydb/src/storage
```

Write `toydb/Cargo.toml`:
```toml
[package]
name = "toydb"
version = "0.1.0"
edition = "2021"

[dependencies]
thiserror = "2"

[dev-dependencies]
rand = "0.8"
```

- [ ] **Step 2: Write error types**

Write `toydb/src/error.rs`:
```rust
use thiserror::Error;

pub type PageId = u32;

#[derive(Error, Debug, PartialEq, Eq)]
pub enum DbError {
    #[error("page not found: {0}")]
    PageNotFound(PageId),

    #[error("key not found")]
    KeyNotFound,

    #[error("page is full")]
    PageFull,

    #[error("checksum mismatch on page {0}")]
    ChecksumMismatch(PageId),

    #[error("I/O error: {0}")]
    IoError(String),
}

pub type Result<T> = std::result::Result<T, DbError>;
```

- [ ] **Step 3: Write lib.rs**

Write `toydb/src/lib.rs`:
```rust
pub mod error;
pub mod storage;

pub use error::{DbError, PageId, Result};
```

Write `toydb/src/main.rs`:
```rust
fn main() {
    println!("toydb - a toy database engine");
}
```

- [ ] **Step 4: Verify it compiles**

```bash
cd /workspace/toydb && cargo build
```
Expected: `Compiling toydb v0.1.0` → success, no errors.

- [ ] **Step 5: Commit**

```bash
git add toydb/Cargo.toml toydb/src/
git commit -m "feat: initialize toydb project with error types"
```

---

## Task 2: Page Layout (Header + Serde)

**Files:**
- Create: `toydb/src/storage/mod.rs`
- Create: `toydb/src/storage/page.rs`

- [ ] **Step 1: Write page module skeleton**

Write `toydb/src/storage/mod.rs`:
```rust
pub mod page;
```

Write `toydb/src/storage/page.rs`:
```rust
use crate::{PageId, Result, DbError};

pub const PAGE_SIZE: usize = 4096;
pub const HEADER_SIZE: usize = 16;

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PageType {
    Inner = 0,
    Leaf = 1,
}

impl PageType {
    pub fn from_u8(v: u8) -> Option<Self> {
        match v {
            0 => Some(Self::Inner),
            1 => Some(Self::Leaf),
            _ => None,
        }
    }
}
```

- [ ] **Step 2: Write PageHeader struct**

Add to `page.rs`:
```rust
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PageHeader {
    pub page_type: u8,
    pub num_cells: u16,
    pub parent_page_id: u32,
    pub next_leaf: u32,
    pub checksum: u32,
    _padding: [u8; 2],
}

impl PageHeader {
    pub fn new(page_type: PageType) -> Self {
        Self {
            page_type: page_type as u8,
            num_cells: 0,
            parent_page_id: 0,
            next_leaf: 0,
            checksum: 0,
            _padding: [0; 2],
        }
    }

    pub fn page_type_enum(&self) -> Option<PageType> {
        PageType::from_u8(self.page_type)
    }

    pub fn serialize(&self) -> [u8; HEADER_SIZE] {
        let mut buf = [0u8; HEADER_SIZE];
        buf[0] = self.page_type;
        buf[1..3].copy_from_slice(&self.num_cells.to_be_bytes());
        buf[3..7].copy_from_slice(&self.parent_page_id.to_be_bytes());
        buf[7..11].copy_from_slice(&self.next_leaf.to_be_bytes());
        // checksum at bytes 11..15
        Ok(())
        buf[11..15].copy_from_slice(&self.checksum.to_be_bytes());
        buf
    }

    pub fn deserialize(buf: &[u8; HEADER_SIZE]) -> Self {
        Self {
            page_type: buf[0],
            num_cells: u16::from_be_bytes([buf[1], buf[2]]),
            parent_page_id: u32::from_be_bytes([buf[3], buf[4], buf[5], buf[6]]),
            next_leaf: u32::from_be_bytes([buf[7], buf[8], buf[9], buf[10]]),
            checksum: u32::from_be_bytes([buf[11], buf[12], buf[13], buf[14]]),
            _padding: [buf[15], 0],  // only 1 padding byte used
        }
    }
}
```

- [ ] **Step 3: Write Page struct**

Add to `page.rs`:
```rust
#[derive(Debug, Clone)]
pub struct Page {
    pub id: PageId,
    pub data: Box<[u8; PAGE_SIZE]>,
    pub is_dirty: bool,
}

impl Page {
    pub fn new(id: PageId, page_type: PageType) -> Self {
        let mut data = Box::new([0u8; PAGE_SIZE]);
        let header = PageHeader::new(page_type);
        let header_bytes = header.serialize();
        data[..HEADER_SIZE].copy_from_slice(&header_bytes);
        Self { id, data, is_dirty: false }
    }

    pub fn header(&self) -> PageHeader {
        let mut hdr_buf = [0u8; HEADER_SIZE];
        hdr_buf.copy_from_slice(&self.data[..HEADER_SIZE]);
        PageHeader::deserialize(&hdr_buf)
    }

    pub fn header_mut(&mut self) -> &mut PageHeader {
        // This is unsafe — we store a mutable reference to reinterpreted bytes.
        // For the toy DB, this is acceptable. Production would use safe accessors.
        unsafe { &mut *(self.data.as_mut_ptr() as *mut PageHeader) }
    }

    pub fn page_type(&self) -> PageType {
        self.header().page_type_enum().unwrap()
    }

    pub fn num_cells(&self) -> u16 {
        self.header().num_cells
    }
}
```

- [ ] **Step 4: Add cell capacity constants**

Add to `page.rs`:
```rust
// Cell sizes (approximate averages for capacity calculation)
pub const INNER_CELL_SIZE: usize = 2 + 8 + 4;    // key_len(u16) + avg_key(8) + child(u32) = 14
pub const LEAF_CELL_SIZE: usize = 2 + 8 + 2 + 256; // key_len + avg_key + val_len + avg_val = 268

// Node capacity derived from page layout
pub const INNER_MAX_CELLS: usize = (PAGE_SIZE - HEADER_SIZE) / INNER_CELL_SIZE;   // ~290
pub const LEAF_MAX_CELLS: usize = (PAGE_SIZE - HEADER_SIZE) / LEAF_CELL_SIZE;     // ~15
```

- [ ] **Step 5: Verify it compiles**

```bash
cd /workspace/toydb && cargo build
```
Expected: compiles without errors.

- [ ] **Step 6: Commit**

```bash
git add toydb/src/storage/
git commit -m "feat: add page layout with header serialization and capacity constants"
```

---

## Task 3: Page Cell Access (Read/Write Key-Value Pairs)

**Files:**
- Modify: `toydb/src/storage/page.rs` (add cell reading/writing methods)

- [ ] **Step 1: Write the test (leaf cell read/write)**

Create `toydb/src/storage/page.rs` test module at the bottom of the file. Add after `impl Page` block:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_leaf_cell_insert_and_read() {
        let mut page = Page::new(1, PageType::Leaf);
        let key = b"hello";
        let value = b"world";

        page.insert_leaf_cell(0, key, value);
        assert_eq!(page.num_cells(), 1);

        let (read_key, read_val) = page.leaf_cell_at(0).unwrap();
        assert_eq!(read_key, key);
        assert_eq!(read_val, value);
    }

    #[test]
    fn test_leaf_cell_multiple_in_order() {
        let mut page = Page::new(1, PageType::Leaf);
        page.insert_leaf_cell(0, b"aaa", b"1");
        page.insert_leaf_cell(1, b"bbb", b"2");
        page.insert_leaf_cell(0, b"000", b"0"); // insert at front, shifts

        assert_eq!(page.num_cells(), 3);
        assert_eq!(page.leaf_cell_at(0).unwrap().0, b"000");
        assert_eq!(page.leaf_cell_at(1).unwrap().0, b"aaa");
        assert_eq!(page.leaf_cell_at(2).unwrap().0, b"bbb");
    }
}
```

- [ ] **Step 2: Run tests (should fail)**

```bash
cd /workspace/toydb && cargo test
```
Expected: test compilation fails — `insert_leaf_cell`, `leaf_cell_at` not defined.

- [ ] **Step 3: Implement leaf cell insertion and reading**

Add to `impl Page` block in `page.rs`:

```rust
/// Cell data starts at byte HEADER_SIZE, grows toward the end.
/// Each leaf cell: [key_len: u16][key: N bytes][value_len: u16][value: M bytes]

fn cell_offset(&self, idx: u16) -> usize {
    let mut offset = HEADER_SIZE;
    for i in 0..idx {
        let key_len = u16::from_be_bytes([self.data[offset], self.data[offset + 1]]) as usize;
        offset += 2 + key_len;
        let val_len = u16::from_be_bytes([self.data[offset], self.data[offset + 1]]) as usize;
        offset += 2 + val_len;
    }
    offset
}

pub fn insert_leaf_cell(&mut self, index: u16, key: &[u8], value: &[u8]) {
    let new_cell_size = 2 + key.len() + 2 + value.len();
    let old_offset = self.cell_offset(index);
    let num = self.num_cells();

    // Shift existing cells right to make room
    let end_offset = self.cell_offset(num);
    self.data.copy_within(old_offset..end_offset, old_offset + new_cell_size);

    // Write new cell at old_offset
    let mut pos = old_offset;
    self.data[pos..pos+2].copy_from_slice(&(key.len() as u16).to_be_bytes());
    pos += 2;
    self.data[pos..pos+key.len()].copy_from_slice(key);
    pos += key.len();
    self.data[pos..pos+2].copy_from_slice(&(value.len() as u16).to_be_bytes());
    pos += 2;
    self.data[pos..pos+value.len()].copy_from_slice(value);

    self.header_mut().num_cells += 1;
}

pub fn leaf_cell_at(&self, index: u16) -> Option<(Vec<u8>, Vec<u8>)> {
    if index >= self.num_cells() { return None; }
    let mut pos = self.cell_offset(index);
    let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
    pos += 2;
    let key = self.data[pos..pos+key_len].to_vec();
    pos += key_len;
    let val_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
    pos += 2;
    let value = self.data[pos..pos+val_len].to_vec();
    Some((key, value))
}

pub fn remove_leaf_cell(&mut self, index: u16) {
    if index >= self.num_cells() { return; }
    let cell_size = {
        let mut pos = self.cell_offset(index);
        let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
        pos += 2 + key_len;
        let val_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
        2 + key_len + 2 + val_len
    };
    let old_offset = self.cell_offset(index);
    let end_offset = self.cell_offset(self.num_cells());
    self.data.copy_within(old_offset + cell_size..end_offset, old_offset);
    // Zero out the now-unused tail
    for i in (end_offset - cell_size)..end_offset {
        self.data[i] = 0;
    }
    self.header_mut().num_cells -= 1;
}
```

- [ ] **Step 4: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: all 2 tests pass.

- [ ] **Step 5: Commit**

```bash
git add toydb/src/storage/page.rs
git commit -m "feat: add leaf cell insert, read, and remove on pages"
```

---

## Task 4: Inner Node Cell Access + lower_bound

**Files:**
- Modify: `toydb/src/storage/page.rs`

- [ ] **Step 1: Write the test**

Add to `mod tests` in `page.rs`:

```rust
#[test]
fn test_inner_cell_insert_and_read() {
    let mut page = Page::new(1, PageType::Inner);
    page.insert_inner_cell(0, b"key10", 42);
    assert_eq!(page.num_cells(), 1);

    let (read_key, child_id) = page.inner_cell_at(0).unwrap();
    assert_eq!(read_key, b"key10");
    assert_eq!(child_id, 42);
}

#[test]
fn test_lower_bound_empty() {
    let page = Page::new(1, PageType::Leaf);
    assert_eq!(page.lower_bound(b"anything"), 0);
}

#[test]
fn test_lower_bound_exact_match() {
    let mut page = Page::new(1, PageType::Inner);
    page.insert_inner_cell(0, b"aaa", 10);
    page.insert_inner_cell(1, b"ccc", 20);
    page.insert_inner_cell(2, b"eee", 30);

    assert_eq!(page.lower_bound(b"aaa"), 0);  // exact match
    assert_eq!(page.lower_bound(b"bbb"), 1);  // between aaa and ccc
    assert_eq!(page.lower_bound(b"ccc"), 1);  // exact match
    assert_eq!(page.lower_bound(b"fff"), 3);  // past end → num_cells
}

#[test]
fn test_inner_child_id_at() {
    let mut page = Page::new(1, PageType::Inner);
    // Inner node: k keys → k+1 children
    // Index i gives child i: keys[i-1] <= child_i's keys < keys[i]
    page.insert_inner_cell(0, b"bbb", 100);
    page.insert_inner_cell(1, b"ddd", 200);

    // child_at(0) → keys < bbb (implicit first child)
    assert_eq!(page.child_page_id_at(0), 0);  // default 0 for unfilled

    // For inner cells, child at idx:
    //   idx=0: first child (left of first key)
    //   idx=1: child after first key (implicit, needs separate struct)
}
```

- [ ] **Step 2: Run tests (should fail)**

```bash
cd /workspace/toydb && cargo test
```
Expected: compilation errors for `insert_inner_cell`, `inner_cell_at`, `lower_bound`.

- [ ] **Step 3: Implement inner cell ops + lower_bound**

Add to `impl Page`:

```rust
/// Inner cell format: [key_len: u16][key: N bytes][child_page_id: u32]
/// Inner node has: first_child_page_id (implicit) + cells
/// child_page_id for cell i is the child RIGHT of that key.

pub fn first_child_page_id(&self) -> PageId {
    // Stored just after header, before first cell
    if self.page_type() != PageType::Inner { return 0; }
    u32::from_be_bytes([self.data[HEADER_SIZE], self.data[HEADER_SIZE+1],
                        self.data[HEADER_SIZE+2], self.data[HEADER_SIZE+3]])
}

pub fn set_first_child_page_id(&mut self, id: PageId) {
    self.data[HEADER_SIZE..HEADER_SIZE+4].copy_from_slice(&id.to_be_bytes());
}

fn inner_cell_offset(&self, idx: u16) -> usize {
    // First 4 bytes after header = first_child_page_id
    let mut offset = HEADER_SIZE + 4;
    for i in 0..idx {
        let key_len = u16::from_be_bytes([self.data[offset], self.data[offset+1]]) as usize;
        offset += 2 + key_len + 4;  // key_len + key + child_page_id
    }
    offset
}

pub fn insert_inner_cell(&mut self, index: u16, key: &[u8], child_id: PageId) {
    let new_cell_size = 2 + key.len() + 4;
    let old_offset = self.inner_cell_offset(index);
    let num = self.num_cells();
    let end_offset = self.inner_cell_offset(num);
    self.data.copy_within(old_offset..end_offset, old_offset + new_cell_size);

    let mut pos = old_offset;
    self.data[pos..pos+2].copy_from_slice(&(key.len() as u16).to_be_bytes());
    pos += 2;
    self.data[pos..pos+key.len()].copy_from_slice(key);
    pos += key.len();
    self.data[pos..pos+4].copy_from_slice(&child_id.to_be_bytes());

    self.header_mut().num_cells += 1;
}

pub fn inner_cell_at(&self, index: u16) -> Option<(Vec<u8>, PageId)> {
    if index >= self.num_cells() { return None; }
    let mut pos = self.inner_cell_offset(index);
    let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
    pos += 2;
    let key = self.data[pos..pos+key_len].to_vec();
    pos += key_len;
    let child_id = u32::from_be_bytes([self.data[pos], self.data[pos+1],
                                        self.data[pos+2], self.data[pos+3]]);
    Some((key, child_id))
}

/// Binary search: first index where key >= search_key
/// For inner nodes, answer gives child index to follow:
///   return 0       → use first_child
///   return i (1..N) → use child of cell i-1
///   return N       → use child of last cell
pub fn lower_bound(&self, search_key: &[u8]) -> u16 {
    let n = self.num_cells() as usize;
    if n == 0 { return 0; }

    let mut lo = 0usize;
    let mut hi = n;
    while lo < hi {
        let mid = (lo + hi) / 2;
        let (key, _) = self.inner_cell_at(mid as u16).unwrap();
        if &key[..] < search_key {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    lo as u16
}

/// For inner nodes: given a key, which child page ID to follow?
pub fn child_page_id_for_key(&self, key: &[u8]) -> PageId {
    let idx = self.lower_bound(key);
    if idx == 0 {
        self.first_child_page_id()
    } else if idx <= self.num_cells() {
        self.inner_cell_at(idx - 1).map(|(_, id)| id).unwrap_or(0)
    } else {
        self.inner_cell_at(self.num_cells() - 1).map(|(_, id)| id).unwrap_or(0)
    }
}
```

- [ ] **Step 4: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add toydb/src/storage/page.rs
git commit -m "feat: add inner node cells and lower_bound binary search"
```

---

## Task 5: MemoryPageStore (In-Memory Page Allocator)

**Files:**
- Create: `toydb/src/storage/mem_store.rs`
- Modify: `toydb/src/storage/mod.rs`

- [ ] **Step 1: Write the test**

Add to `mem_store.rs` (create file first with `#[cfg(test)] mod tests { ... }`):

```rust
use super::*;
use crate::storage::page::{PageType, Page};

#[test]
fn test_allocate_and_read_back() {
    let store = MemoryPageStore::new();
    let id = store.allocate_page().unwrap();
    assert_eq!(id, 1);  // first page gets ID 1

    let mut page = Page::new(id, PageType::Leaf);
    page.insert_leaf_cell(0, b"hello", b"world");
    store.write_page(&page).unwrap();

    let read = store.read_page(id).unwrap();
    assert_eq!(read.id, id);
    assert_eq!(read.num_cells(), 1);
    assert_eq!(read.leaf_cell_at(0).unwrap().0, b"hello");
}

#[test]
fn test_free_and_reuse() {
    let store = MemoryPageStore::new();
    let id1 = store.allocate_page().unwrap();
    let id2 = store.allocate_page().unwrap();
    store.free_page(id1).unwrap();
    let id3 = store.allocate_page().unwrap();
    assert_eq!(id3, id1);  // reuses freed page
}

#[test]
fn test_read_nonexistent_page() {
    let store = MemoryPageStore::new();
    let result = store.read_page(999);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), DbError::PageNotFound(999)));
}
```

- [ ] **Step 2: Run tests (should fail)**

```bash
cd /workspace/toydb && cargo test
```
Expected: compilation errors — `MemoryPageStore` not defined.

- [ ] **Step 3: Write MemoryPageStore**

Write `toydb/src/storage/mem_store.rs`:
```rust
use std::sync::{Mutex, atomic::{AtomicU32, Ordering}};
use crate::{PageId, Result, DbError};
use crate::storage::page::Page;

/// In-memory page store for Phase 1 testing.
/// All pages live in a HashMap. No persistence.
pub struct MemoryPageStore {
    pages: Mutex<std::collections::HashMap<PageId, Page>>,
    next_id: AtomicU32,
    freelist: Mutex<Vec<PageId>>,
}

impl MemoryPageStore {
    pub fn new() -> Self {
        Self {
            pages: Mutex::new(std::collections::HashMap::new()),
            next_id: AtomicU32::new(1),
            freelist: Mutex::new(Vec::new()),
        }
    }

    pub fn read_page(&self, id: PageId) -> Result<Page> {
        let pages = self.pages.lock().unwrap();
        pages.get(&id).cloned().ok_or(DbError::PageNotFound(id))
    }

    pub fn write_page(&self, page: &Page) -> Result<()> {
        let mut pages = self.pages.lock().unwrap();
        pages.insert(page.id, page.clone());
        Ok(())
    }

    pub fn allocate_page(&self) -> Result<PageId> {
        // Reuse from freelist if available
        if let Some(id) = self.freelist.lock().unwrap().pop() {
            return Ok(id);
        }
        let id = self.next_id.fetch_add(1, Ordering::SeqCst);
        Ok(id)
    }

    pub fn free_page(&self, id: PageId) -> Result<()> {
        let mut pages = self.pages.lock().unwrap();
        pages.remove(&id);
        self.freelist.lock().unwrap().push(id);
        Ok(())
    }
}
```

Update `toydb/src/storage/mod.rs`:
```rust
pub mod page;
pub mod mem_store;
```

- [ ] **Step 4: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: all MemoryPageStore tests pass.

- [ ] **Step 5: Commit**

```bash
git add toydb/src/storage/
git commit -m "feat: add in-memory page store for testing"
```

---

## Task 6: B+Tree — Empty Tree and Single Insert/Lookup

**Files:**
- Create: `toydb/src/storage/btree.rs`
- Modify: `toydb/src/storage/mod.rs`

- [ ] **Step 1: Write the test**

Write `toydb/src/storage/btree.rs` with test module:

```rust
use crate::{Result};
use crate::storage::page::PageType;

pub struct BPlusTree {
    pub root_page_id: u32,
    pub store: std::sync::Arc<MemoryPageStore>,
    pub leaf_max_cells: usize,
    pub inner_max_cells: usize,
}

impl BPlusTree {
    pub fn new(store: std::sync::Arc<MemoryPageStore>) -> Self {
        Self {
            root_page_id: 0,
            store,
            leaf_max_cells: super::page::LEAF_MAX_CELLS,
            inner_max_cells: super::page::INNER_MAX_CELLS,
        }
    }
}

use crate::storage::mem_store::MemoryPageStore;

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Arc;

    fn setup() -> BPlusTree {
        BPlusTree::new(Arc::new(MemoryPageStore::new()))
    }

    #[test]
    fn test_empty_tree_lookup_returns_none() {
        let tree = setup();
        let result = tree.lookup(b"anything").unwrap();
        assert!(result.is_none());
    }

    #[test]
    fn test_insert_single_and_lookup() {
        let mut tree = setup();
        tree.insert(b"key1", b"val1").unwrap();
        assert_eq!(tree.lookup(b"key1").unwrap(), Some(b"val1".to_vec()));
    }

    #[test]
    fn test_lookup_missing_key() {
        let mut tree = setup();
        tree.insert(b"key1", b"val1").unwrap();
        assert!(tree.lookup(b"key2").unwrap().is_none());
    }
}
```

- [ ] **Step 2: Run tests (should fail)**

```bash
cd /workspace/toydb && cargo test
```
Expected: compilation errors — `lookup`, `insert` not defined.

- [ ] **Step 3: Implement empty-tree creation and single insert/lookup**

Add to `impl BPlusTree`:

```rust
pub fn lookup(&self, key: &[u8]) -> Result<Option<Vec<u8>>> {
    if self.root_page_id == 0 {
        return Ok(None);
    }
    let mut page = self.store.read_page(self.root_page_id)?;

    // Traverse inner nodes
    while page.page_type() == PageType::Inner {
        let child_id = page.child_page_id_for_key(key);
        page = self.store.read_page(child_id)?;
    }

    // At leaf
    let idx = page.lower_bound(key);
    if let Some((k, v)) = page.leaf_cell_at(idx) {
        if &k[..] == key {
            return Ok(Some(v));
        }
    }
    Ok(None)
}

pub fn insert(&mut self, key: &[u8], value: &[u8]) -> Result<()> {
    if self.root_page_id == 0 {
        // Empty tree — create first leaf as root
        let page_id = self.store.allocate_page()?;
        let mut leaf = Page::new(page_id, PageType::Leaf);
        leaf.insert_leaf_cell(0, key, value);
        self.store.write_page(&leaf)?;
        self.root_page_id = page_id;
        return Ok(());
    }

    // Traverse to leaf (no path tracking for single insert — no split handling yet)
    let mut page = self.store.read_page(self.root_page_id)?;
    while page.page_type() == PageType::Inner {
        let child_id = page.child_page_id_for_key(key);
        page = self.store.read_page(child_id)?;
    }

    // Insert into leaf (assumes space — split not yet implemented)
    let idx = page.lower_bound(key) as u16;
    let mut leaf = self.store.read_page(page.id)?;
    leaf.insert_leaf_cell(idx, key, value);
    self.store.write_page(&leaf)?;
    Ok(())
}
```

- [ ] **Step 4: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: 3 btree tests pass.

- [ ] **Step 5: Commit**

```bash
git add toydb/src/storage/
git commit -m "feat: add B+Tree with single insert and lookup"
```

---

## Task 7: B+Tree — Leaf Split

**Files:**
- Modify: `toydb/src/storage/btree.rs`

- [ ] **Step 1: Write the leaf split test**

Add to btree test module:

```rust
#[test]
fn test_leaf_split() {
    let mut tree = setup();
    // Insert enough keys to fill a leaf beyond capacity
    for i in 0..(tree.leaf_max_cells + 1) as u32 {
        let key = format!("key{:03}", i);
        tree.insert(key.as_bytes(), &i.to_be_bytes()).unwrap();
    }

    // All keys should be retrievable
    for i in 0..(tree.leaf_max_cells + 1) as u32 {
        let key = format!("key{:03}", i);
        let val = tree.lookup(key.as_bytes()).unwrap().unwrap();
        assert_eq!(val, i.to_be_bytes());
    }
}

#[test]
fn test_leaf_split_ordering() {
    let mut tree = setup();
    let n = tree.leaf_max_cells + 5;
    for i in 0..n as u32 {
        tree.insert(&i.to_be_bytes(), &i.to_be_bytes()).unwrap();
    }
    // Verify all retrievable
    for i in 0..n as u32 {
        assert_eq!(tree.lookup(&i.to_be_bytes()).unwrap().unwrap(), i.to_be_bytes());
    }
}
```

- [ ] **Step 2: Run tests (should fail)**

```bash
cd /workspace/toydb && cargo test
```
Expected: test_leaf_split panics — `PageFull` error when inserting beyond capacity.

- [ ] **Step 3: Implement leaf split with root split**

Replace the `insert` method with this version:

```rust
pub fn insert(&mut self, key: &[u8], value: &[u8]) -> Result<()> {
    if self.root_page_id == 0 {
        let page_id = self.store.allocate_page()?;
        let mut leaf = Page::new(page_id, PageType::Leaf);
        leaf.insert_leaf_cell(0, key, value);
        self.store.write_page(&leaf)?;
        self.root_page_id = page_id;
        return Ok(());
    }

    // Traverse, collecting path
    let mut path: Vec<(u32, Page)> = Vec::new();
    let mut page = self.store.read_page(self.root_page_id)?;
    while page.page_type() == PageType::Inner {
        let child_id = page.child_page_id_for_key(key);
        path.push((page.id, page));
        page = self.store.read_page(child_id)?;
    }

    // Try to insert into leaf
    let mut leaf = self.store.read_page(page.id)?;
    let idx = page.lower_bound(key) as u16;
    if (leaf.num_cells() as usize) < self.leaf_max_cells {
        leaf.insert_leaf_cell(idx, key, value);
        self.store.write_page(&leaf)?;
        return Ok(());
    }

    // Leaf is full — split
    let (mut left, mut right, separator_key) = self.split_leaf(&mut leaf, idx, key, value);

    // Propagate split upward
    let mut right_page_id = right.id;
    let mut sep = separator_key;

    loop {
        match path.pop() {
            None => {
                // Root split — create new root
                let new_root_id = self.store.allocate_page()?;
                let mut new_root = Page::new(new_root_id, PageType::Inner);
                new_root.set_first_child_page_id(left.id);
                new_root.insert_inner_cell(0, &sep, right_page_id);
                self.store.write_page(&new_root)?;
                self.root_page_id = new_root_id;
                return Ok(());
            }
            Some((parent_id, mut parent)) => {
                if (parent.num_cells() as usize) < self.inner_max_cells {
                    parent.insert_inner_cell(parent.num_cells(), &sep, right_page_id);
                    self.store.write_page(&parent)?;
                    self.store.write_page(&left)?;
                    self.store.write_page(&right)?;
                    return Ok(());
                }
                // Inner node also full — split it
                let result = self.split_inner(&mut parent, &sep, right_page_id);
                left = result.0;
                right = result.1;
                sep = result.2;
                right_page_id = right.id;
            }
        }
    }
}

fn split_leaf(&self, leaf: &mut Page, insert_idx: u16, key: &[u8], value: &[u8]) -> (Page, Page, Vec<u8>) {
    let new_page_id = self.store.allocate_page().unwrap();
    let mut new_leaf = Page::new(new_page_id, PageType::Leaf);

    let mid = self.leaf_max_cells / 2;
    // Move cells from leaf to new_leaf
    for i in (mid..leaf.num_cells()).rev() {
        let (k, v) = leaf.leaf_cell_at(i).unwrap();
        new_leaf.insert_leaf_cell(0, &k, &v);
        leaf.remove_leaf_cell(i);
    }

    // Determine where to insert the new key-value
    let separator_key: Vec<u8>;
    let new_key_insert_idx = if insert_idx < mid { insert_idx } else { insert_idx - mid };

    if insert_idx < mid {
        leaf.insert_leaf_cell(insert_idx, key, value);
        separator_key = new_leaf.leaf_cell_at(0).unwrap().0;
    } else {
        new_leaf.insert_leaf_cell(new_key_insert_idx, key, value);
        separator_key = new_leaf.leaf_cell_at(0).unwrap().0;
    }

    // Link leaves for range scan
    new_leaf.header_mut().next_leaf = leaf.header().next_leaf;
    leaf.header_mut().next_leaf = new_page_id;

    (leaf.clone(), new_leaf, separator_key)
}

fn split_inner(&self, inner: &mut Page, insert_key: &[u8], child_page_id: PageId) -> (Page, Page, Vec<u8>) {
    let new_page_id = self.store.allocate_page().unwrap();
    let mut new_inner = Page::new(new_page_id, PageType::Inner);

    let mid = self.inner_max_cells / 2;
    // The middle key gets pushed up
    let separator_key = inner.inner_cell_at(mid as u16).unwrap().0;

    // Move cells [mid+1..] to new_inner
    for i in ((mid + 1)..inner.num_cells()).rev() {
        let (k, child_id) = inner.inner_cell_at(i).unwrap();
        new_inner.insert_inner_cell(0, &k, child_id);
        inner.remove_inner_cell(i);
    }
    // Remove the middle key (it goes up)
    inner.remove_inner_cell(mid as u16);

    // Set new_inner's first child to the child of the removed middle key
    let mid_child = inner.inner_cell_at(mid as u16 - 1).map(|(_, id)| id).unwrap_or(0);
    // Actually, the child of the removed middle key should be the first child of new_inner
    let (_, mid_key_child) = inner.inner_cell_at(mid as u16).unwrap_or((vec![], 0));
    new_inner.set_first_child_page_id(mid_key_child);

    // Insert the new key into appropriate inner node
    if insert_key < &separator_key[..] {
        let idx = inner.lower_bound(insert_key);
        inner.insert_inner_cell(idx, insert_key, child_page_id);
    } else {
        let idx = new_inner.lower_bound(insert_key);
        new_inner.insert_inner_cell(idx, insert_key, child_page_id);
    }

    (inner.clone(), new_inner, separator_key)
}
```

Add `remove_inner_cell` to `page.rs`:

```rust
pub fn remove_inner_cell(&mut self, index: u16) {
    if index >= self.num_cells() { return; }
    let cell_size = {
        let mut pos = self.inner_cell_offset(index);
        let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
        2 + key_len + 4
    };
    let old_offset = self.inner_cell_offset(index);
    let end_offset = self.inner_cell_offset(self.num_cells());
    self.data.copy_within(old_offset + cell_size..end_offset, old_offset);
    for i in (end_offset - cell_size)..end_offset {
        self.data[i] = 0;
    }
    self.header_mut().num_cells -= 1;
}
```

- [ ] **Step 4: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: all tests pass, including leaf split tests.

- [ ] **Step 5: Commit**

```bash
git add toydb/src/storage/
git commit -m "feat: add B+Tree leaf split with root split propagation"
```

---

## Task 8: B+Tree — Root Split

**Files:**
- Modify: `toydb/src/storage/btree.rs`

- [ ] **Step 1: Write root split test**

Add to btree test module:

```rust
#[test]
fn test_root_split_increases_height() {
    let mut tree = setup();
    // Fill root (which is a leaf initially), cause root split
    let n = tree.leaf_max_cells + 1;
    for i in 0..n as u32 {
        tree.insert(&i.to_be_bytes(), &i.to_be_bytes()).unwrap();
    }

    // Root should now be an inner node
    let root = tree.store.read_page(tree.root_page_id).unwrap();
    assert_eq!(root.page_type(), PageType::Inner);
    assert!(root.num_cells() >= 1);

    // All keys still retrievable
    for i in 0..n as u32 {
        assert_eq!(tree.lookup(&i.to_be_bytes()).unwrap().unwrap(), i.to_be_bytes());
    }
}

#[test]
fn test_multi_level_split() {
    let mut tree = setup();
    // Insert enough keys to force at least 2 levels of inner nodes
    let n = tree.leaf_max_cells * tree.inner_max_cells / 2;
    for i in 0..n as u32 {
        tree.insert(&i.to_be_bytes(), &i.to_be_bytes()).unwrap();
    }
    // Spot check
    assert_eq!(tree.lookup(&0u32.to_be_bytes()).unwrap().unwrap(), 0u32.to_be_bytes());
    assert_eq!(tree.lookup(&((n-1) as u32).to_be_bytes()).unwrap().unwrap(), ((n-1) as u32).to_be_bytes());
}
```

- [ ] **Step 2: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: both tests pass (root split was already implemented in Task 7).

- [ ] **Step 3: Commit**

```bash
git add toydb/src/storage/btree.rs
git commit -m "test: add root split and multi-level split tests"
```

---

## Task 9: B+Tree — Delete

**Files:**
- Modify: `toydb/src/storage/btree.rs`

- [ ] **Step 1: Write delete tests**

Add to btree test module:

```rust
#[test]
fn test_delete_existing_key() {
    let mut tree = setup();
    tree.insert(b"key1", b"val1").unwrap();
    tree.insert(b"key2", b"val2").unwrap();

    tree.delete(b"key1").unwrap();

    assert!(tree.lookup(b"key1").unwrap().is_none());
    assert_eq!(tree.lookup(b"key2").unwrap().unwrap(), b"val2");
}

#[test]
fn test_delete_nonexistent_key() {
    let mut tree = setup();
    tree.insert(b"key1", b"val1").unwrap();

    // Deleting nonexistent key should not error
    tree.delete(b"key2").unwrap();

    assert_eq!(tree.lookup(b"key1").unwrap().unwrap(), b"val1");
}

#[test]
fn test_delete_all_and_reinsert() {
    let mut tree = setup();
    let keys: Vec<Vec<u8>> = (0..10).map(|i| i.to_be_bytes().to_vec()).collect();

    for k in &keys {
        tree.insert(k, k).unwrap();
    }
    for k in &keys {
        tree.delete(k).unwrap();
    }
    for k in &keys {
        assert!(tree.lookup(k).unwrap().is_none());
    }
    // Re-insert should work
    for k in &keys {
        tree.insert(k, k).unwrap();
    }
    for k in &keys {
        assert_eq!(tree.lookup(k).unwrap().unwrap(), k);
    }
}
```

- [ ] **Step 2: Run tests (should fail)**

```bash
cd /workspace/toydb && cargo test
```
Expected: compilation error — `delete` not defined.

- [ ] **Step 3: Implement delete (allow underflow)**

Add to `impl BPlusTree`:

```rust
/// Delete a key. Allows underflow (node may have fewer than min keys).
/// This is a deliberate simplification — production B+Trees merge or
/// redistribute underfull nodes.
pub fn delete(&mut self, key: &[u8]) -> Result<()> {
    if self.root_page_id == 0 {
        return Ok(());
    }

    let mut page = self.store.read_page(self.root_page_id)?;
    while page.page_type() == PageType::Inner {
        let child_id = page.child_page_id_for_key(key);
        page = self.store.read_page(child_id)?;
    }

    // At leaf — find and remove
    let idx = page.lower_bound(key);
    if let Some((k, _)) = page.leaf_cell_at(idx) {
        if &k[..] == key {
            let mut leaf = self.store.read_page(page.id)?;
            leaf.remove_leaf_cell(idx);
            self.store.write_page(&leaf)?;
        }
    }
    Ok(())
}
```

- [ ] **Step 4: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: all delete tests pass.

- [ ] **Step 5: Commit**

```bash
git add toydb/src/storage/btree.rs
git commit -m "feat: add B+Tree delete with underflow allowed"
```

---

## Task 10: B+Tree — Range Scan

**Files:**
- Modify: `toydb/src/storage/btree.rs`

- [ ] **Step 1: Write range scan tests**

Add to btree test module:

```rust
#[test]
fn test_range_scan_single_leaf() {
    let mut tree = setup();
    for i in 0..5u32 {
        tree.insert(&i.to_be_bytes(), &i.to_be_bytes()).unwrap();
    }

    let results = tree.range_scan(&2u32.to_be_bytes(), &4u32.to_be_bytes()).unwrap();
    assert_eq!(results.len(), 3);
    assert_eq!(results[0].1, 2u32.to_be_bytes());
    assert_eq!(results[2].1, 4u32.to_be_bytes());
}

#[test]
fn test_range_scan_across_leaves() {
    let mut tree = setup();
    // Insert enough keys to guarantee multiple leaves
    let n = tree.leaf_max_cells * 2;
    for i in 0..n as u32 {
        tree.insert(&i.to_be_bytes(), &i.to_be_bytes()).unwrap();
    }

    let start = (tree.leaf_max_cells - 2) as u32;
    let end = (tree.leaf_max_cells + 2) as u32;
    let results = tree.range_scan(&start.to_be_bytes(), &end.to_be_bytes()).unwrap();
    assert_eq!(results.len(), 5); // keys leaf_max-2 .. leaf_max+2 inclusive
}

#[test]
fn test_range_scan_empty_range() {
    let mut tree = setup();
    tree.insert(b"aaa", b"1").unwrap();
    tree.insert(b"ccc", b"2").unwrap();

    let results = tree.range_scan(b"bbb", b"bbb").unwrap();
    assert!(results.is_empty());
}

#[test]
fn test_range_scan_beyond_end() {
    let mut tree = setup();
    tree.insert(b"aaa", b"1").unwrap();

    let results = tree.range_scan(b"zzz", b"zzz").unwrap();
    assert!(results.is_empty());
}
```

- [ ] **Step 2: Run tests (should fail)**

```bash
cd /workspace/toydb && cargo test
```
Expected: compilation error — `range_scan` not defined.

- [ ] **Step 3: Implement range_scan**

Add to `impl BPlusTree`:

```rust
pub fn range_scan(&self, start: &[u8], end: &[u8]) -> Result<Vec<(Vec<u8>, Vec<u8>)>> {
    let mut results = Vec::new();
    if self.root_page_id == 0 {
        return Ok(results);
    }

    // Find starting leaf
    let mut page = self.store.read_page(self.root_page_id)?;
    while page.page_type() == PageType::Inner {
        let child_id = page.child_page_id_for_key(start);
        page = self.store.read_page(child_id)?;
    }

    // Walk leaf chain
    loop {
        for i in 0..page.num_cells() {
            let (key, val) = page.leaf_cell_at(i).unwrap();
            if &key[..] > end {
                return Ok(results);
            }
            if &key[..] >= start {
                results.push((key, val));
            }
        }

        let next_id = page.header().next_leaf;
        if next_id == 0 {
            break;
        }
        page = self.store.read_page(next_id)?;
    }
    Ok(results)
}
```

- [ ] **Step 4: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: all range scan tests pass.

- [ ] **Step 5: Commit**

```bash
git add toydb/src/storage/btree.rs
git commit -m "feat: add B+Tree range scan via leaf chain traversal"
```

---

## Task 11: Page Checksums

**Files:**
- Modify: `toydb/src/storage/page.rs`

- [ ] **Step 1: Write checksum test**

Add to page test module:

```rust
#[test]
fn test_checksum_validation() {
    let mut page = Page::new(1, PageType::Leaf);
    page.insert_leaf_cell(0, b"hello", b"world");

    let checksum = page.compute_checksum();
    page.header_mut().checksum = checksum;

    assert!(page.verify_checksum());

    // Corrupt data
    page.data[100] ^= 0xFF;
    assert!(!page.verify_checksum());
}
```

- [ ] **Step 2: Run tests (should fail)**

```bash
cd /workspace/toydb && cargo test
```
Expected: compilation errors — `compute_checksum`, `verify_checksum` not defined.

- [ ] **Step 3: Implement CRC32 checksums**

Add `crc32fast` to `Cargo.toml` dependencies:
```toml
crc32fast = "1"
```

Add to `impl Page`:
```rust
pub fn compute_checksum(&self) -> u32 {
    // Checksum all bytes EXCEPT the checksum field itself (bytes 11-15)
    let mut hasher = crc32fast::Hasher::new();
    hasher.update(&self.data[..11]);
    hasher.update(&self.data[15..]);
    hasher.finalize()
}

pub fn verify_checksum(&self) -> bool {
    let stored = self.header().checksum;
    let computed = self.compute_checksum();
    stored == computed
}
```

- [ ] **Step 4: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: checksum test passes.

- [ ] **Step 5: Commit**

```bash
git add toydb/Cargo.toml toydb/src/storage/page.rs
git commit -m "feat: add CRC32 page checksums for corruption detection"
```

---

## Task 12: Property-Based Random Tests

**Files:**
- Modify: `toydb/src/storage/btree.rs`

- [ ] **Step 1: Write property-based test**

Add to btree test module:

```rust
#[cfg(test)]
mod property_tests {
    use super::*;
    use std::sync::Arc;
    use rand::Rng;

    #[test]
    fn test_random_inserts_and_lookups() {
        let mut tree = BPlusTree::new(Arc::new(MemoryPageStore::new()));
        let mut rng = rand::thread_rng();
        let mut inserted: Vec<(Vec<u8>, Vec<u8>)> = Vec::new();

        // Insert 500 random KV pairs
        for _ in 0..500 {
            let key_len = rng.gen_range(1..32);
            let key: Vec<u8> = (0..key_len).map(|_| rng.gen()).collect();
            let val_len = rng.gen_range(1..64);
            let val: Vec<u8> = (0..val_len).map(|_| rng.gen()).collect();
            tree.insert(&key, &val).unwrap();
            inserted.push((key, val));
        }

        // Verify all can be looked up
        for (key, val) in &inserted {
            let found = tree.lookup(key).unwrap();
            assert_eq!(found.as_ref(), Some(val), "key {:?} not found", key);
        }
    }

    #[test]
    fn test_random_inserts_and_deletes() {
        let mut tree = BPlusTree::new(Arc::new(MemoryPageStore::new()));
        let mut rng = rand::thread_rng();
        let mut keys: Vec<Vec<u8>> = Vec::new();

        for _ in 0..200 {
            let key: Vec<u8> = (0..8).map(|_| rng.gen()).collect();
            tree.insert(&key, &key).unwrap();
            keys.push(key);
        }

        // Delete random half
        for key in keys.iter().take(100) {
            tree.delete(key).unwrap();
        }

        // Verify remaining keys still exist, deleted ones don't
        for (i, key) in keys.iter().enumerate() {
            let found = tree.lookup(key).unwrap();
            if i < 100 {
                assert!(found.is_none(), "deleted key {:?} still present", key);
            } else {
                assert!(found.is_some(), "kept key {:?} missing", key);
            }
        }
    }

    #[test]
    fn test_b_tree_invariants() {
        let mut tree = BPlusTree::new(Arc::new(MemoryPageStore::new()));
        let mut rng = rand::thread_rng();

        for _ in 0..500 {
            let key: Vec<u8> = (0..8).map(|_| rng.gen()).collect();
            tree.insert(&key, &key).unwrap();
        }

        // Invariant: all leaves at same depth
        let depth = tree.verify_leaf_depth();
        assert!(depth.is_ok(), "leaf depth invariant violated: {:?}", depth.err());

        // Invariant: no node exceeds max cells
        tree.verify_node_capacity().unwrap();
    }
}
```

- [ ] **Step 2: Run tests (should fail)**

```bash
cd /workspace/toydb && cargo test
```
Expected: compilation errors — `verify_leaf_depth`, `verify_node_capacity` not defined.

- [ ] **Step 3: Implement invariant checks**

Add to `impl BPlusTree`:

```rust
/// Verify all leaves are at the same depth. Returns Ok(depth).
pub fn verify_leaf_depth(&self) -> Result<usize> {
    if self.root_page_id == 0 { return Ok(0); }
    let root = self.store.read_page(self.root_page_id)?;
    let mut depths = Vec::new();
    self.collect_leaf_depths(&root, 0, &mut depths)?;

    let first = depths.first().copied().unwrap_or(0);
    for d in &depths {
        if *d != first {
            return Err(DbError::IoError(format!(
                "leaf depth mismatch: expected {}, found {}", first, d
            )));
        }
    }
    Ok(first)
}

fn collect_leaf_depths(&self, page: &Page, depth: usize, depths: &mut Vec<usize>) -> Result<()> {
    if page.page_type() == PageType::Leaf {
        depths.push(depth);
        return Ok(());
    }
    if page.first_child_page_id() != 0 {
        let child = self.store.read_page(page.first_child_page_id())?;
        self.collect_leaf_depths(&child, depth + 1, depths)?;
    }
    for i in 0..page.num_cells() {
        if let Some((_, child_id)) = page.inner_cell_at(i) {
            if child_id != 0 {
                let child = self.store.read_page(child_id)?;
                self.collect_leaf_depths(&child, depth + 1, depths)?;
            }
        }
    }
    Ok(())
}

pub fn verify_node_capacity(&self) -> Result<()> {
    if self.root_page_id == 0 { return Ok(()); }
    let root = self.store.read_page(self.root_page_id)?;
    self.verify_node_capacity_recursive(&root)
}

fn verify_node_capacity_recursive(&self, page: &Page) -> Result<()> {
    let max = if page.page_type() == PageType::Leaf {
        self.leaf_max_cells
    } else {
        self.inner_max_cells
    };
    if page.num_cells() as usize > max {
        return Err(DbError::IoError(format!(
            "page {} has {} cells, max is {}", page.id, page.num_cells(), max
        )));
    }
    if page.page_type() == PageType::Inner {
        if page.first_child_page_id() != 0 {
            let child = self.store.read_page(page.first_child_page_id())?;
            self.verify_node_capacity_recursive(&child)?;
        }
        for i in 0..page.num_cells() {
            if let Some((_, child_id)) = page.inner_cell_at(i) {
                if child_id != 0 {
                    let child = self.store.read_page(child_id)?;
                    self.verify_node_capacity_recursive(&child)?;
                }
            }
        }
    }
    Ok(())
}
```

- [ ] **Step 4: Run tests (should pass)**

```bash
cd /workspace/toydb && cargo test
```
Expected: all property-based tests pass.

- [ ] **Step 5: Commit**

```bash
git add toydb/src/storage/btree.rs
git commit -m "test: add random property-based tests and B+Tree invariant checks"
```

---

## Self-Review

**1. Spec coverage:** 
- [x] Page layout (16-byte header, 4096-byte pages) — Task 2
- [x] Inner and Leaf node structures — Task 2, 3, 4
- [x] Cell serialization (variable-length) — Task 3, 4
- [x] Lookup algorithm — Task 6
- [x] Insert with split propagation — Task 7, 8
- [x] Delete (allow underflow) — Task 9
- [x] Range scan via leaf chain — Task 10
- [x] Page checksums — Task 11
- [x] MemoryPageStore — Task 5
- [x] Order derived from page layout — Task 2 (constants)
- [x] Lower bound binary search — Task 4
- [x] Property-based random testing — Task 12
- [ ] Tree height correct, fan-out verified — covered by root split test (Task 8)
- [ ] Root split handled — Task 8

Gap: `inner_cell_offset` uses `u16::from_be_bytes` without importing `FromBeBytes` trait — this is fine since Rust provides `from_be_bytes` on primitive types directly.

**2. Placeholder scan:** No TBDs, TODOs, or "implement later" found. All steps have concrete code.

**3. Type consistency:** 
- `PageId = u32` used consistently
- `BPlusTree` struct fields used uniformly across all tasks
- `LocationBound` returns `u16`, used as index in all cell methods
- `Page::leaf_cell_at(idx)` and `Page::inner_cell_at(idx)` take `u16` consistently
