# Toy Linux Kernel — Gotchas

## 1. Cross-Compilation Hell

**Problem:** Building x86 ELF binaries on macOS (Mach-O) or ARM needs a cross-compiler.

**Solution:**
```bash
# On macOS:
brew install i386-elf-gcc i386-elf-binutils
# Or use Docker (recommended — see Dockerfile)

# RISC-V:
brew install riscv64-elf-gcc riscv64-elf-binutils
```

**Gotcha:** macOS `gcc` is actually `clang`. Use explicit `TOOLPREFIX`:
```bash
make TOOLPREFIX=i386-elf-
```

## 2. Bootloader Assembly Bugs

**Problem:** Boot sector (`bootasm.S`) errors cause silent boot failures.

**Common issues:**
- Forgetting to set code segment after switching to 32-bit protected mode
- A20 gate not enabled → odd megabyte wraps around to 0
- Stack pointer (`%esp`) not set before calling C code
- GDT entries misaligned or wrong segment limits

**Debug tip:** Use QEMU monitor (`Ctrl-Alt-2`):
```
info registers
x/10i $eip
```

## 3. Memory Paging Nightmares

**Problem:** Identity mapping vs virtual address confusion.

**Key insight:** In xv6, physical memory is mapped twice:
1. Low virtual addresses: identity mapped (VIRTUAL == PHYSICAL) — used during boot
2. High virtual addresses: KERNBASE offset (VIRTUAL = PHYSICAL + KERNBASE) — normal operation

**Gotcha:** After enabling paging, ALL addresses become virtual. If you stored a physical address in a pointer, it's now wrong.

**Common bug:** Accessing memory that hasn't been mapped in the page table → page fault.

```
Checklist:
□ Is the page present? (PTE_P)
□ Does the process have permission? (PTE_U for user, cleared for kernel-only)
□ Is the page allocated? (kalloc succeeded?)
□ Is the page table entry pointing to the right physical page?
```

## 4. Process Scheduling Races

**Problem:** Concurrent trap handling + context switching.

**xv6 approach:** One global big kernel lock (BKL). Simplifies reasoning but limits concurrency.

**Gotchas:**
- `yield()` must be called with ptable lock held
- `sched()` must be called from inside a spinlock
- Never return from `sched()` without re-acquiring the lock

**Context switch sequence (hardest part):**
```
User Process A → trap → sched → swtch → scheduler → swtch → sched → trapret → User Process B
```

## 5. Buffer Cache Synchronization

**Problem:** Multiple threads accessing the same disk block.

**xv6 solution:** `bread()` blocks until target block's buffer is released. Each buffer has a sleeping lock.

**Gotcha:** Double-`brelse()` corrupts the refcount. Buffer recycling can evict a buffer still in use.

```c
// CORRECT pattern:
struct buf *b = bread(dev, blockno);
// ... use b->data ...
brelse(b);
// NEVER touch b after brelse()
```

## 6. File System Crash Consistency

**Problem:** Power loss during multi-block updates leaves file system corrupted.

**xv6's logging approach:**
1. `begin_op()` — wait if log is committing
2. Write all modified blocks to log (on disk!)
3. Write commit record
4. Install (copy log blocks to actual locations)
5. Clear log
6. `end_op()`

**Gotcha:** Log size is limited. If a transaction exceeds log size, split it or panic.

## 7. Interrupt Handling Race Conditions

**Problem:** Interrupts arriving during critical sections.

**xv6 solution:** `pushcli()` / `popcli()` disable interrupts. Spinlocks implicitly disable interrupts on x86.

**Gotcha:** Long-running code inside `pushcli` delays all interrupts (including timer → no scheduling).

**Rule:** Never sleep (call `sleep()` or disk I/O) while holding a spinlock.

## 8. ELF Loading Issues

**Problem:** Exec format errors cause mysterious crashes.

**Common issues:**
- ELF magic number check failing (wrong endianness?)
- `loaduvm` reading past file end
- Stack guard page setup wrong (user-accessible guard → security hole)
- argv/envp strings not null-terminated

## 9. User-Kernel Boundary

**Problem:** Kernel accessing user memory directly is unsafe.

**xv6 functions:** `fetchint()`, `fetchstr()`, `argint()`, `argstr()` — safe wrappers.

**Gotcha:** Directly dereferencing a user pointer in kernel code → kernel panic if user passed invalid address.

## 10. Lock Ordering

**Problem:** Deadlock from inconsistent lock acquisition order.

**xv6 lock hierarchy (MUST follow):**
1. `ptable.lock` (process table)
2. `inode.lock` (per-inode)
3. `buf.lock` (per-buffer, held by bread/bwrite internally)
4. `log.lock`

**Never:** Hold `ptable.lock` then try to acquire `inode.lock` → deadlock with someone holding `inode.lock` trying to wake up a process.

## 11. Fork Copy-on-Write Trap

**Problem:** Simple `fork()` copies all parent memory → slow, wastes memory.

**xv6 approach:** Eager copy (simple but wasteful). Real kernels use COW.

**Acceptable for toy:** Just copy everything. Not a bug, just a design choice.

## 12. QEMU GDB Debugging

```bash
# Run with GDB stub:
make qemu-gdb

# In another terminal:
gdb kernel
(gdb) target remote localhost:26000
(gdb) break main
(gdb) continue
```
