# Toy Linux Kernel — Specification

> Based on community implementations: xv6, xv6-riscv, xv6-annotated

## References

| Project | Stars | Language | Description |
|---------|-------|----------|-------------|
| [xv6-public](https://github.com/mit-pdos/xv6-public) | 9.3K | C, Assembly | MIT's reimplementation of Unix V6 for x86 multiprocessor |
| [xv6-riscv](https://github.com/mit-pdos/xv6-riscv) | — | C, Assembly | RISC-V port (current/maintained version) |
| [xv6-annotated](https://github.com/palladian1/xv6-annotated) | — | — | Line-by-line guide through xv6 kernel code |
| OSTEP Textbook | — | — | Operating Systems: Three Easy Pieces (Remzi Arpaci-Dusseau) |

## Architecture Overview

```
┌─────────────────────────────────────────────┐
│                  User Space                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │   init   │  │   sh     │  │  user    │  │
│  │ (process)│  │ (shell)  │  │ programs │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  │
│       │             │             │         │
├───────┼─────────────┼─────────────┼─────────┤
│       │   System Calls (trap gate) │         │
├───────┴─────────────┴─────────────┴─────────┤
│                  Kernel Space                │
│  ┌──────────────────────────────────────┐   │
│  │         Process Manager              │   │
│  │  (scheduler, fork, exec, wait, exit) │   │
│  ├──────────────────────────────────────┤   │
│  │         Memory Manager               │   │
│  │  (kalloc, vm, paging, page tables)   │   │
│  ├──────────────────────────────────────┤   │
│  │         File System                  │   │
│  │  (bio, fs, inodes, directories)      │   │
│  ├──────────────────────────────────────┤   │
│  │         Trap/Interrupt Handler       │   │
│  │  (vectors, syscall dispatch)         │   │
│  ├──────────────────────────────────────┤   │
│  │         Drivers                      │   │
│  │  (disk, console, keyboard)           │   │
│  └──────────────────────────────────────┘   │
├─────────────────────────────────────────────┤
│              Hardware (QEMU)                │
│    CPU (x86 or RISC-V), RAM, Disk, UART     │
└─────────────────────────────────────────────┘
```

## Feature Specification

### 1. Bootloader

**Source files (xv6):** `bootasm.S`, `bootmain.c`

- Real mode → 32-bit protected mode transition
- Set up GDT (Global Descriptor Table)
- Enable A20 line
- Load kernel ELF from disk into memory
- Jump to kernel entry point

**Milestones:**
- [x] BIOS loads boot sector (512 bytes)
- [x] Switch from 16-bit real mode to 32-bit protected mode
- [x] Load kernel from disk using IDE/ATA PIO mode
- [x] Parse ELF header and load segments
- [x] Jump to `_start` / `main`

### 2. Memory Management

**Source files:** `kalloc.c`, `vm.c`, `mmu.h`

**Physical Memory:**
- Physical memory allocator (`kalloc`, `kfree`)
- Free page linked list
- Page size: 4096 bytes (4KB)

**Virtual Memory (x86):**
- Two-level page table (Page Directory → Page Table → 4KB page)
- Kernel address space: `KERNBASE = 0x80000000` (2GB)
- User address space: `0x00000000 → KERNBASE`
- Each process has own page directory
- Kernel mappings identical across all processes (top 2GB)
- `mappages()` — create PTEs for virtual→physical mapping
- `allocuvm()` — grow user virtual memory (page tables + physical pages)
- `deallocuvm()` — shrink user virtual memory
- `copyuvm()` — copy memory image for fork

**Key structures:**
- `struct kmap` — kernel virtual memory layout
- Page Directory Entry (PDE), Page Table Entry (PTE)
- Flags: `PTE_P` (present), `PTE_W` (writable), `PTE_U` (user accessible)

### 3. Process Management

**Source files:** `proc.c`, `proc.h`, `swtch.S`

**Process states:**
```
UNUSED → EMBRYO → SLEEPING → RUNNABLE → RUNNING → ZOMBIE
                   ↑  ↑                    |
                   └──└────────────────────┘
```

**Features:**
- Process table (`struct proc` array, NPROC = 64)
- Per-process kernel stack
- Context switching (`swtch` — saves/restores registers)
- Round-robin scheduler
- `fork()` — copy parent process (copy-on-write not required)
- `exec()` — replace process image with new program
- `wait()` — wait for child to exit
- `exit()` — terminate process
- `kill()` — send signal
- `sleep()` / `wakeup()` — synchronization primitives

**struct proc fields:**
- `sz` — process memory size
- `pgdir` — page directory (uint32 * on x86, pagetable_t on riscv)
- `kstack` — kernel stack pointer
- `state` — UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE
- `pid` — process ID
- `parent` — parent process pointer
- `tf` — trap frame (saved registers)
- `context` — scheduler context (callee-saved registers)
- `chan` — sleep channel (for sleep/wakeup)
- `cwd` — current working directory (inode ref)

### 4. Synchronization

**Source files:** `spinlock.c`, `sleeplock.c`

- **Spinlocks:** `acquire()`, `release()` — for short critical sections, disables interrupts on x86
- **Sleeplocks:** `acquiresleep()`, `releasesleep()` — for long operations (disk I/O), process sleeps while waiting

### 5. File System

**Source files:** `bio.c`, `fs.c`, `file.c`, `sysfile.c`

**7-layer design:**

| Layer | Description | Files |
|-------|-------------|-------|
| Disk | IDE disk reads/writes | `ide.c` |
| Buffer Cache | Synchronize disk block access, LRU cache | `bio.c` |
| Logging | Atomic multi-block updates (crash recovery) | `log.c` |
| Inodes | Unnamed files, block allocation | `fs.c` |
| Directories | Named files, directory entries | `fs.c` |
| Pathnames | Hierarchical lookup (`/usr/rtm/xv6/fs.c`) | `fs.c` |
| File Descriptors | Unix file API (open, read, write, close) | `file.c` |

**Buffer cache:**
- `struct buf` — in-memory disk block
- `bread()` — read block (blocks if another thread holds it)
- `bwrite()` — write block to disk
- `brelse()` — release buffer
- LRU eviction via linked list

**Inode structure:**
- `dinode` (on disk): type, major/minor, nlink, size, addrs[NDIRECT+1] (12 direct + 1 indirect)
- `inode` (in memory): dev, inum, ref, valid + dinode fields
- `iget()`, `iput()` — reference counting
- `readi()`, `writei()` — read/write file data
- `dirlookup()` — search directory for filename
- `ialloc()` — allocate new inode
- `balloc()` — allocate new data block

**Directory entry (16 bytes):**
```c
struct dirent {
  ushort inum;      // inode number
  char name[DIRSIZ]; // filename (14 chars)
};
```

### 6. System Calls

**Source file:** `syscall.c`, `sysproc.c`, `sysfile.c`

| # | System Call | Description |
|---|-------------|-------------|
| 1 | `fork()` | Create child process |
| 2 | `exit()` | Terminate process |
| 3 | `wait()` | Wait for child |
| 4 | `pipe()` | Create pipe |
| 5 | `read()` | Read from fd |
| 6 | `kill()` | Send signal |
| 7 | `exec()` | Replace process image |
| 8 | `fstat()` | Get file status |
| 9 | `chdir()` | Change directory |
| 10 | `dup()` | Duplicate fd |
| 11 | `getpid()` | Get process ID |
| 12 | `sbrk()` | Grow heap |
| 13 | `sleep()` | Sleep seconds |
| 14 | `uptime()` | System uptime |
| 15 | `open()` | Open file |
| 16 | `write()` | Write to fd |
| 17 | `mknod()` | Create device file |
| 18 | `unlink()` | Remove file |
| 19 | `link()` | Create hard link |
| 20 | `mkdir()` | Create directory |
| 21 | `close()` | Close fd |

### 7. Interrupts and Traps

**Source files:** `trap.c`, `vectors.pl`

- IDT (Interrupt Descriptor Table) with 256 entries
- Trap gate for system calls (int 0x80 or similar)
- Hardware interrupt handlers (timer, keyboard, disk)
- `trap()` — dispatch handler based on trap number

## Development Roadmap

### Phase 1: Bootstrap (Week 1-2)
- Set up cross-compiler toolchain
- Boot sector → load kernel → enter main()
- Console output (`cprintf`)
- QEMU integration

### Phase 2: Memory Management (Week 3-4)
- Physical page allocator
- Page tables and virtual memory
- Kernel page directory setup
- User address space creation

### Phase 3: Processes (Week 5-6)
- Process table and PCB
- Context switching
- Scheduler (round-robin)
- fork/exec/wait/exit
- System call infrastructure

### Phase 4: File System (Week 7-8)
- Disk driver (IDE)
- Buffer cache
- Inodes and block allocation
- Directory and pathname lookup
- File descriptor API

### Phase 5: User Space (Week 9-10)
- init process (first user process)
- Shell (`sh`)
- User programs (`cat`, `echo`, `ls`, `rm`, `mkdir`)
- C library (`ulib.c`, `usys.S`)

## Build and Run

```bash
# Build cross-compiler (x86)
make TOOLPREFIX=i386-jos-elf-

# Build and run in QEMU
make qemu

# RISC-V version
make qemu  # uses riscv64-unknown-elf-gcc
```

## Success Criteria

1. Kernel boots in QEMU and prints to console
2. Can run multiple user processes with time-sliced scheduling
3. Processes can allocate memory dynamically (sbrk)
4. Can create, read, write, and delete files
5. Shell (`sh`) runs and can execute user programs
6. Parent can fork, child can exec, parent can wait
7. System survives out-of-memory conditions gracefully
