# Toy Database — Gotchas

## 1. B+ Tree Split Cascading to Root

**Problem:** Inserting into a full leaf → split → parent full → split → ... → root split → tree grows taller.

**Handling:**
```
leaf split → new leaf page
  → insert separator key into parent
    → if parent full:
      → split parent → new inner node
        → insert separator into grandparent
          → ... cascades until a node has room
            → if root splits: create new root (tree height +1)
```

**Gotcha:** Root split is the ONLY case where tree height increases. Must handle specially.

**Off-by-one in split point:** For a node with N keys (full), split at ⌊N/2⌋:
- Left gets keys[0..mid-1] (mid keys)
- Right gets keys[mid+1..N-1] (remaining keys)
- Middle key keys[mid] goes up to parent
- For leaf split, middle key is COPIED up (stays in right leaf too)
- For inner split, middle key is MOVED up (removed from children)

## 2. B+ Tree Merge/Redistribute After Delete (Advanced)

**Problem:** Deleting keys can leave nodes underfull (< ⌈m/2⌉ keys).

**Redistribution** (if sibling has spare keys):
```
┌───┬───┐        ┌───┬───┬───┐
│ A │ B │        │ C │ D │ E │
└───┴───┘        └───┴───┴───┘
    parent key = X
After move:
┌───┬───┬───┐    ┌───┬───┐
│ A │ B │ C │    │ D │ E │
└───┴───┴───┘    └───┴───┘
    parent key = D (updated!)
```

**Merge** (if neither sibling has spares):
```
┌───┐            ┌───┐
│ A │            │ C │
└───┘            └───┘
    parent key = B
After merge:
┌───┬───┬───┐
│ A │ B │ C │
└───┴───┴───┘
(parent loses key B, one fewer child)
```

**Toy DB simplification:** Allow underflow. Accept that sparse trees waste space. Much simpler.

## 3. Latch Deadlock

**Problem:** Two threads latching nodes in opposite order → deadlock.

```
Thread 1: latch(node_A, SHARED) → latch(node_B, EXCLUSIVE)  // blocks
Thread 2: latch(node_B, SHARED) → latch(node_A, EXCLUSIVE)  // blocks
```

**Solution: ALWAYS acquire latches top-down (root → leaf).** Never acquire a parent after a child.

**But:** Optimistic approach releases shared latch on parent before upgrading child. This breaks the protocol? No — because we re-acquire child latch exclusively. The key is we never go UP the tree to acquire latches.

## 4. Optimistic Insert Must Handle Restart

**Problem:** Optimistic insert acquires shared latches to leaf, upgrades leaf to exclusive, discovers it needs to split. But parent latch was already released!

**Solution:** When unsafe condition detected after optimistic traversal:
1. Release ALL latches
2. Restart from root, acquiring EXCLUSIVE latches on unsafe nodes

**Performance:** Restart is rare (only when node is full). Most inserts just update leaf and return.

## 5. Page Cache Eviction Race Conditions

**Problem:** Thread 1 fixes a page. Thread 2 tries to evict it (pin count = 1 → should fail).

**Buffer manager duties:**
- Track `pin_count` per page
- Only evict pages with `pin_count == 0`
- Track `dirty` flag — flush before eviction

**Gotcha:** After `unfix_page()`, the page pointer is INVALID. Thread must not access it.

## 6. WAL Partial Write Recovery

**Problem:** Power failure during WAL write → truncated log entry → recovery fails.

**Solution:** Each WAL entry has a checksum. During recovery:
```
while read_log_entry():
    if checksum_valid(entry):
        apply_redo(entry)
    else:
        break  // truncated entry, stop recovery
```

**Gotcha:** WAL must be synced to disk (fsync/O_DIRECT) before acknowledging commit. Otherwise, "committed" transaction lost on crash.

## 7. Serialization Format

**Problem:** In-memory structs vs on-disk bytes must be consistent.

**Issues:**
- Endianness: big-endian vs little-endian (use network byte order = big-endian for portability)
- Integer sizes: use fixed-width (uint32, uint64 — never `int` or `size_t`)
- String encoding: length-prefixed UTF-8
- Struct padding: compiler may add padding between fields → `#pragma pack(1)` or manual serialize

**Safe approach:** Serialize/deserialize manually (never memcpy struct to disk):
```c
void serialize_page(Page* p, uint8_t* buf) {
    buf[0] = p->type;
    *(uint16_t*)(buf+1) = htons(p->num_keys);
    // ... write each key/value explicitly
}
```

## 8. Binary Search in B+ Tree Node

**Problem:** Binary search in a sorted array of keys within a node.

**Gotcha:** `lower_bound` returns index of first key >= search_key. Not the same as exact match.

```
keys = [5, 12, 20]
search for 7  → lower_bound = 1 (key 12 >= 7)
search for 12 → lower_bound = 1 (key 12 >= 12)
search for 25 → lower_bound = 3 (past end → last child)
```

**For inner nodes:** `lower_bound(key)` gives the child index to follow.
- If key < keys[0] → child[0]
- If keys[i] <= key < keys[i+1] → child[i+1]
- If key >= keys[last] → child[last+1]

## 9. Leaf Chain Maintenance

**Problem:** When a leaf splits, the leaf chain (next_leaf pointers) must be updated.

```
Before: leaf_A.next → leaf_C
Split: leaf_A splits into leaf_A + leaf_B
After:  leaf_A.next → leaf_B.next → leaf_C
```

**Gotcha:** Next-leaf pointer update must be atomic with the split. If crash between split and pointer update → broken chain → range scan misses data.

## 10. Concurrent Split + Read

**Problem:** Thread 1 is splitting a node. Thread 2 is reading the same node.

**Why latch coupling works:** Thread 1 holds exclusive latch on the node being split. Thread 2 holds shared latch on the PARENT. When Thread 1 finishes split and inserts separator into parent, parent is also latched → Thread 2's parent latch blocks Thread 1 until Thread 2 reaches child. Once Thread 2 requests child latch, it blocks waiting for Thread 1's exclusive latch to be released.

## 11. Duplicate Key Handling

**Problem:** What if user inserts same key twice?

**Options:**
1. **Replace** (unique key): overwrite old value. Most K/V stores do this.
2. **Append** (allow duplicates): store both values. Need way to iterate duplicates.
3. **Reject** (error): return "key exists".

**Toy DB:** Replace on duplicate key (simplest).

## 12. Testing the B+ Tree

**Test checklist:**
- [ ] Insert 1 key → lookup works
- [ ] Insert enough to cause leaf split → verify split point
- [ ] Insert enough to cause root split → verify new root
- [ ] Delete keys → verify underflow allowed (toy) or merge (full)
- [ ] Range scan across multiple leaves
- [ ] Range scan on single leaf
- [ ] Empty tree operations (delete on empty, lookup on empty)
- [ ] Duplicate key insert → verify replace behavior
- [ ] Concurrent readers on different leaves (no blocking)
- [ ] Concurrent reader + writer on same leaf (reader blocks or gets old version)
