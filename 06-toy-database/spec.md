# Toy Database — Specification

> Based on: toydb, baobab, jcsherin/btree, Let's Build a Simple Database

## References

| Project | Stars | Language | Key Features |
|---------|-------|----------|--------------|
| [toydb](https://github.com/polyrabbit/toydb) | — | Rust | Distributed SQL DB — B+tree, MVCC, Raft |
| [baobab](https://github.com/oryankibandi/baobab) | — | Go | B+ Tree K/V store, buffer manager, TinyLFU, HTTP API |
| [jcsherin/btree](https://github.com/jcsherin/btree) | — | Go | Thread-safe in-memory B+Tree, Bayer-Schkolnick concurrency |
| [db_tutorial](https://github.com/cstack/db_tutorial) | — | C | "Let's Build a Simple Database" — SQLite clone from scratch |

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│                     Query Interface                   │
│               (Put, Get, Delete, RangeScan)           │
├─────────────────────────────────────────────────────┤
│                   B+ Tree Index                       │
│  ┌─────────┐    ┌─────────┐    ┌──────────┐         │
│  │ Inner   │───►│ Inner   │───►│  Leaf    │         │
│  │  Node   │    │  Node   │    │  Node    │         │
│  │[k1|k2|k3]│   │[k4|k5|k6]│   │[v1|v2|v3]│         │
│  └─────────┘    └─────────┘    └──────────┘         │
│  Concurrent Read/Write via Latch Coupling            │
├─────────────────────────────────────────────────────┤
│                   Buffer Manager                      │
│         (Page cache, LRU/W-TinyLFU eviction)          │
├─────────────────────────────────────────────────────┤
│                    Disk Manager                       │
│         (Page allocation, free list, I/O)             │
├─────────────────────────────────────────────────────┤
│              Write-Ahead Log (WAL)                    │
│         (Crash recovery, atomicity)                   │
└─────────────────────────────────────────────────────┘
```

## Feature Specification

### 1. B+ Tree Structure

**Node types:**

```
INNER NODE                         LEAF NODE
┌──────────────────┐              ┌──────────────────┐
│ type: INNER      │              │ type: LEAF       │
│ num_keys: 3      │              │ num_keys: 3      │
│ keys: [5, 12, 20]│              │ keys: [1, 3, 5]  │
│ children:        │              │ values: ["a","b",│
│   [page_10,      │              │         "c"]     │
│    page_11,      │              │ next_leaf: 42    │
│    page_12,      │              └──────────────────┘
│    page_13]      │
└──────────────────┘
```

**Key properties:**
- Order `m` B+ Tree: each node has ⌈m/2⌉ to `m` keys
- Inner nodes: `k` keys → `k+1` child pointers
- Leaf nodes: `k` keys → `k` values + next-leaf pointer (for range scans)
- All leaves at same depth
- Root can have fewer than ⌈m/2⌉ keys

**Page layout (4096 bytes per page):**
```
[Header: 16 bytes][Key-Value/Child pairs: variable][Free space]
Header:
  - page_type: 1 byte (INNER=0, LEAF=1)
  - num_keys: 2 bytes
  - parent_page_id: 4 bytes
  - next_leaf (leaf only): 4 bytes
  - checksum: 4 bytes
  - padding: 1 byte
```

### 2. B+ Tree Operations

**Lookup(key) → value | None:**
```
function lookup(key):
    node = read_page(root_page_id)
    while node.type == INNER:
        i = binary_search(node.keys, key)  // first key >= key
        child_page_id = node.children[i]
        node = read_page(child_page_id)
    // At leaf
    i = binary_search(node.keys, key)
    if i < node.num_keys and node.keys[i] == key:
        return node.values[i]
    return None
```

**Insert(key, value):**
```
function insert(key, value):
    if root is None:
        root = new_leaf([key], [value])
        return

    path = traverse_to_leaf(root, key)
    leaf = path.last()

    if leaf has space:
        insert_into_leaf(leaf, key, value)
    else:
        new_leaf = split_leaf(leaf)
        insert_into_leaf(appropriate_leaf, key, value)
        // Propagate split up
        separator_key = new_leaf.keys[0]
        while path has parent:
            parent = path.pop()
            if parent has space:
                insert_into_inner(parent, separator_key, new_leaf_page_id)
                return
            else:
                new_parent = split_inner(parent)
                insert_into_inner(appropriate_parent, separator_key, new_leaf_page_id)
                separator_key = middle_key
                new_leaf_page_id = new_parent
        // Root split
        new_root = new_inner([separator_key], [old_root, new_root_page_id])
```

**Delete(key):** (simplified — allow underflow, no merge/redistribute)
```
function delete(key):
    leaf = find_leaf(root, key)
    remove_key_from_leaf(leaf, key)
    // Allow underflow (simplified for toy DB)
```

### 3. Concurrency Control — Latch Coupling

**Goal:** Allow concurrent reads while a write is in progress. Allow concurrent writes on different branches.

**Latch types:**
- **Shared latch (read lock):** Multiple readers can hold simultaneously
- **Exclusive latch (write lock):** Only one writer, blocks all others

**Bayer-Schkolnick Protocol (Safe Node Optimization):**

```
function lookup_with_latches(key, mode=READ):
    # Always top-down acquisition
    latch(root, SHARED)
    node = root

    while node.type == INNER:
        child = node.children[binary_search(node.keys, key)]
        latch(child, SHARED)
        if mode == READ or is_safe(child, mode):
            unlatch(node)       # Release parent!
        node = child

    # At leaf — if writing, upgrade to exclusive
    if mode == WRITE:
        unlatch(node)           # Release shared
        latch(node, EXCLUSIVE)  # Re-acquire exclusive
        if leaf will split:     # Oops, unsafe
            unlatch(node)
            return RETRY_WITH_EXCLUSIVE  # Restart from root
    # Do operation
    unlatch(node)
```

**Safe node definition:**
- For INSERT: node has room for one more key (won't split)
- For DELETE: node has more than min keys (won't underflow)

**Key insight:** Most inserts/deletes don't cause splits. Optimistic approach (shared latches down, upgrade at leaf) works 90% of cases. Fall back to pessimistic (exclusive latches from root) for the 10%.

### 4. Buffer Manager

**Purpose:** Cache frequently accessed pages in memory.

**Interface:**
```
fix_page(page_id, mode=READ) → page*
unfix_page(page_id, dirty=false)
```

**Eviction policy: W-TinyLFU**
- Window cache (1% of total): admits new pages
- Main cache: TinyLFU frequency filter + LRU
- Probationary segment: pages demoted from main

**Simpler alternative: Clock (second-chance) algorithm**
```
clock_hand = 0
function evict_page():
    while True:
        page = buffer_pool[clock_hand]
        if page.reference_bit == 0:
            if page.is_dirty: write_to_disk(page)
            return page
        else:
            page.reference_bit = 0
        clock_hand = (clock_hand + 1) % pool_size
```

### 5. Disk Manager

**Interface:**
```
allocate_page() → page_id
free_page(page_id)
read_page(page_id) → byte[4096]
write_page(page_id, byte[4096])
```

**Free list:** Track freed pages for reuse. Store as linked list in file header or bitmap.

### 6. Write-Ahead Log (WAL)

**Format:**
```
[Log Sequence Number: 8 bytes][Transaction ID: 4 bytes][Page ID: 4 bytes]
[Offset: 2 bytes][Data Length: 2 bytes][Before Image: N bytes][After Image: N bytes]
[Checksum: 4 bytes]
```

**Recovery:**
1. On startup, check for WAL file
2. Replay all committed transactions (redo log)
3. Truncate WAL after checkpoint

### 7. Indexing

**Primary index:** The B+ Tree itself (clustered by key).

**Secondary indexes:** Additional B+ Trees mapping `(secondary_key → primary_key)`.

**Range scan using leaf chain:**
```
function range_scan(start_key, end_key):
    leaf = find_leaf(root, start_key)
    results = []
    while leaf != None:
        for (k, v) in leaf.pairs:
            if k > end_key: return results
            if k >= start_key: results.append((k, v))
        leaf = read_page(leaf.next_leaf)
    return results
```

## Development Roadmap

### Phase 1: Page Manager (Week 1)
- Page layout and serialization
- Disk read/write (file I/O)
- Free list

### Phase 2: B+ Tree (Single-Threaded) (Week 2-3)
- Node structures (InnerNode, LeafNode)
- Lookup with binary search
- Insert with split
- Delete (allow underflow)
- Range scan (leaf chain)

### Phase 3: Buffer Manager (Week 4)
- Fix/unfix page API
- LRU or Clock eviction
- Dirty page tracking + background writer

### Phase 4: Concurrency (Week 5-6)
- Read-Write latches per node
- Latch coupling (Bayer-Schkolnick)
- Optimistic insert with restart
- Thread safety validation

### Phase 5: WAL + Recovery (Week 7)
- Write-ahead log format
- Crash recovery (replay)
- Checkpoint mechanism

### Phase 6: Query Interface (Week 8)
- Put/Get/Delete API
- Range queries
- Simple REPL or HTTP API

## Page Size and Fan-out Calculation

With 4KB pages:
- Inner node: ~14 byte per key + 8 byte child pointer = ~180 keys per page
- Leaf node: ~14 byte per key + 256 byte value = ~15 entries per page

With order 180, storing 1M records:
- Height 2: 180² = 32,400 leaves → ~500K records
- Height 3: 180³ = 5.8M leaves → ~87M records

## Success Criteria

1. Insert 100K key-value pairs and verify all retrievable
2. Delete subset and verify remaining keys intact
3. Range scan returns correct ordered results
4. Two concurrent readers can scan simultaneously
5. Writer doesn't block readers on different branches
6. Kill process mid-write, restart, verify WAL recovery
7. Buffer manager hit rate >80% on repeated lookups
8. B+ Tree depth grows correctly as data scales (no premature splits)
