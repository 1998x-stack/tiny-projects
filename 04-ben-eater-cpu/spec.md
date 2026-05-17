# Ben Eater 8-bit Breadboard CPU — Specification

> Based on: Ben Eater's tutorial series (eater.net/8bit), jamesbates/jcpu

## References

| Resource | Description |
|----------|-------------|
| [eater.net/8bit](https://eater.net/8bit) | Ben Eater's complete tutorial — videos, schematics, parts list |
| [Complete Parts List](https://eater.net/8bit/parts) | Full bill of materials (~$250-300 USD) |
| [Schematics (KiCad)](https://eater.net/8bit/schematics) | All module schematics, PCB layout |
| [jcpu](https://github.com/jamesbates/jcpu) | Open-source: Arduino EEPROM programmer, assembler, schematic files |

## Architecture: SAP-1 (Simple As Possible)

```
                          ┌──────────┐
                          │   CLOCK  │ (555 Timer)
                          │  Module  │
                          └────┬─────┘
                               │ CLK signal to all modules
                               │
    ┌──────────┐         ┌─────┴─────┐         ┌──────────┐
    │ PROGRAM  │   addr   │  MEMORY   │  data   │ INSTRUCT.│
    │ COUNTER  ├────────►│ ADDR REG  ├────────►│ REGISTER │
    │  (PC)    │         │  (MAR)    │         │   (IR)   │
    └────┬─────┘         └─────┬─────┘         └────┬─────┘
         │                     │                     │
         │               ┌─────┴─────┐               │ opcode
         │               │    RAM    │               │
         │               │ (74189×2) │               │
         │               │ 16 bytes  │               │
         │               └─────┬─────┘               │
         │                     │                     │
         │                     ▼                     ▼
         │               ┌──────────┐          ┌──────────┐
         │               │    BUS   │◄─────────│ CONTROL  │
         │               │  (8-bit) │          │  LOGIC   │
         │               └────┬─────┘          │ (EEPROMs)│
         │                    │                └──────────┘
         │         ┌──────────┼──────────┐
         │         ▼          ▼          ▼
         │   ┌──────────┐┌──────────┐┌──────────┐
         │   │    A     ││    B     ││  OUTPUT  │
         │   │ REGISTER ││ REGISTER ││ REGISTER │
         │   │(74LS173) ││(74LS173) ││(74LS273) │
         │   └────┬─────┘└────┬─────┘└────┬─────┘
         │        │           │           │
         │        └─────┬─────┘           ▼
         │              │            ┌──────────┐
         │         ┌────┴────┐       │ 7-SEGMENT│
         │         │   ALU   │       │ DISPLAY  │
         │         │(74LS283)│       │  (4 dig) │
         │         └────┬────┘       └──────────┘
         │              │
         │              ▼
         │         ┌──────────┐
         │         │  FLAGS   │
         │         │ REGISTER │
         │         │(CARRY, 0)│
         │         └──────────┘
         │
         └────────────┘
```

## Complete Bill of Materials

### Breadboards & Wiring
| Qty | Item |
|-----|------|
| 14 | Breadboard (830 tie-points, quality recommended) |
| 1 | 22 AWG solid tinned-copper hook-up wire (assorted colors) |
| 2 | Power rail modules (or use breadboard rails) |

### Resistors
| Qty | Value | Purpose |
|-----|-------|---------|
| 10 | 1kΩ | LED current limiting, pull-up |
| 9 | 10kΩ | Pull-up/down |
| 24 | 470Ω | LED current limiting |
| 1 | 100kΩ | 555 timing |
| 1 | 1MΩ | 555 timing |
| 1 | 1MΩ potentiometer | Clock speed adjust |

### Capacitors
| Qty | Value | Purpose |
|-----|-------|---------|
| 6 | 0.01µF | Decoupling |
| 16 | 0.1µF | Decoupling (one per IC!) |
| 1 | 1µF | 555 timing |

### Integrated Circuits (TTL 74LS Series)

| Qty | Part # | Description | Module |
|-----|--------|-------------|--------|
| 4 | NE555 | Timer IC | Clock |
| 2 | 74LS00 | Quad NAND gate | Control |
| 1 | 74LS02 | Quad NOR gate | Control |
| 5 | 74LS04 | Hex inverter | Various |
| 3 | 74LS08 | Quad AND gate | Control |
| 1 | 74LS32 | Quad OR gate | Control |
| 1 | 74LS107 | Dual JK flip-flop | Clock |
| 2 | 74LS86 | Quad XOR gate | ALU |
| 1 | 74LS138 | 3-to-8 decoder | Control |
| 1 | 74LS139 | Dual 2-to-4 decoder | Control |
| 4 | 74LS157 | Quad 2-to-1 selector | Bus muxing |
| 2 | 74LS161 | 4-bit sync counter | Program Counter, Micro-step |
| 8 | 74LS173 | 4-bit D-register (3-state) | A, B, IR, MAR |
| 2 | 74189 | 64-bit RAM (16×4) | RAM (16 bytes) |
| 6 | 74LS245 | Octal bus transceiver | Bus interface |
| 1 | 74LS273 | Octal D flip-flop | Output register |
| 2 | 74LS283 | 4-bit binary full adder | ALU |
| 3 | 28C16 | 2K×8 EEPROM | Control logic microcode |

### Display & I/O
| Qty | Item |
|-----|------|
| 44 | Red LED (bus + status) |
| 8 | Yellow LED |
| 12 | Green LED |
| 21 | Blue LED |
| 4 | Common cathode 7-segment display |
| 3 | Double-throw toggle switch |
| 3 | Momentary 6mm tact switch |
| 1 | 8-position DIP switch |
| 1 | 4-position DIP switch |

### Programming Tool
| Qty | Item |
|-----|------|
| 1 | Arduino Nano (EEPROM programmer) |
| 2 | 74HC595 shift register |

### Power Supply
- 5V DC regulated power supply (≥ 2A)
- USB power bank works for testing
- Barrel jack adapter for breadboard

## Module-by-Module Build Plan

### Module 1: Clock Module
**ICs:** 555 Timer × 3, 74LS04, 74LS08, 74LS32, 74LS107

**Features:**
- Astable mode: continuous clock pulse
- Monostable mode: single pulse (manual step)
- Bistable mode: halt/run (toggle switch)
- Adjustable speed via potentiometer (~1Hz to ~500Hz)
- Clean debounced output

### Module 2: Registers (A, B)
**ICs:** 74LS173 × 4 (2 per register)
- 8-bit storage
- 3-state outputs (can disconnect from bus)
- Load on clock edge

### Module 3: ALU (Arithmetic Logic Unit)
**ICs:** 74LS283 × 2, 74LS86 × 2, 74LS245
- 8-bit addition (two 4-bit adders cascaded)
- Subtraction via 2's complement (XOR + carry in)
- Output to bus via 74LS245 transceiver
- Flags: Carry out

### Module 4: RAM (16 bytes)
**ICs:** 74189 × 2 (16 × 4-bit each = 16 × 8-bit)
- 16 addressable bytes (4-bit address bus)
- DIP switches for manual programming
- Read/Write control

### Module 5: Memory Address Register (MAR)
**ICs:** 74LS173 × 2
- 4-bit register (only 16 bytes of RAM)
- Holds current memory address

### Module 6: Program Counter
**ICs:** 74LS161 × 2
- 4-bit counter (16 steps)
- Increment, load, reset
- Output to bus

### Module 7: Instruction Register
**ICs:** 74LS173 × 2
- Holds current instruction opcode
- Upper 4 bits → opcode → Control Logic
- Lower 4 bits → operand

### Module 8: Output Register + Display
**ICs:** 74LS273, EEPROM 28C16
- Latch output value
- Drive 7-segment displays (4 digits, multiplexed or via EEPROM decoder)
- Signed/unsigned decimal display

### Module 9: Control Logic (Microcode EEPROMs)
**ICs:** 28C16 × 3, 74LS138, 74LS139

**EEPROM address inputs:**
- 4 bits: instruction opcode (from IR)
- 3 bits: micro-step counter (from 74LS161 ring counter)
- 2 bits: flags (carry, zero)

**EEPROM data outputs (16 control signals):**
| Signal | Controls |
|--------|----------|
| HLT | Halt clock |
| MI | MAR in (load from bus) |
| RI | RAM in (data from bus to RAM) |
| RO | RAM out (data to bus) |
| IO | Instruction register out (lower 4 bits to bus) |
| II | Instruction register in (load from bus) |
| AI | A register in |
| AO | A register out |
| BI | B register in |
| EO | ALU out (to bus) |
| SU | ALU subtract mode |
| OI | Output register in |
| CE | Program counter enable (count) |
| CO | Program counter out |
| J | Jump (load PC from bus) |
| FI | Flags in (latch carry/zero) |

## Instruction Set Architecture

### 8-bit Instruction Format
```
[ opcode (4 bits) ][ operand/data (4 bits) ]
```

### Instructions

| Mnemonic | Opcode | Operand | Description |
|----------|--------|---------|-------------|
| NOP | 0000 | — | No operation |
| LDA | 0001 | addr | Load A from RAM[addr] |
| ADD | 0010 | addr | A = A + RAM[addr] |
| SUB | 0011 | addr | A = A - RAM[addr] |
| STA | 0100 | addr | Store A to RAM[addr] |
| LDI | 0101 | value | Load A with immediate value |
| JMP | 0110 | addr | Jump to addr |
| JC  | 0111 | addr | Jump if carry |
| JZ  | 1000 | addr | Jump if zero |
| OUT | 1110 | — | Output A register to display |
| HLT | 1111 | — | Halt execution |

### Micro-Instruction Sequence (per instruction)

**Fetch cycle (T0-T2, common to all instructions):**
- T0: CO|MI — PC→MAR (load PC value into MAR)
- T1: RO|II|CE — RAM[MAR]→IR, PC++
- T2: (decode) — IR upper 4 bits → control logic

**Execute cycles (instruction-specific):**

Example — **LDA addr (0001):**
- T3: IO|MI — IR(lower 4 bits)→MAR
- T4: RO|AI — RAM[MAR]→A register

Example — **ADD addr (0010):**
- T3: IO|MI — IR(lower 4 bits)→MAR
- T4: RO|BI — RAM[MAR]→B register
- T5: EO|AI — ALU result→A register

### Example Program: Fibonacci

```asm
; Calculate Fibonacci sequence, display results
LDI 0       ; A = 0
STA 14      ; Store F(0) in RAM[14]
OUT         ; Display 0
LDI 1       ; A = 1
STA 15      ; Store F(1) in RAM[15]
OUT         ; Display 1

LOOP:
LDA 14      ; A = F(n-2)
ADD 15      ; A = F(n-2) + F(n-1)
OUT         ; Display result
JC  END     ; If overflow, stop
STA 15      ; F(n-1) becomes F(n)
LDA 15      ; (move to proper location)
STA 14      ; F(n) becomes F(n-1) for next iteration
JMP LOOP

END:
HLT
```

## EEPROM Programming

Using Arduino Nano as programmer:
1. Wire EEPROM to Arduino (address pins, data pins, WE, OE, CE)
2. Write Arduino sketch to program microcode
3. Each of the 3 EEPROMs stores 8 of the 16 control signals
4. Address = {opcode(4) + step(3) + flags(2)} = 9 bits
5. Data = control signals (8 bits per EEPROM)

**Arduino programmer reference:** jamesbates/jcpu — `arduino/Microcode/`

## Development Roadmap

### Phase 1: Clock + Basic Logic (Week 1)
- Build clock module
- Test with LEDs
- Learn 555 timer modes

### Phase 2: Registers + ALU (Week 2-3)
- Build A and B registers
- Build ALU
- Test add/subtract operations manually

### Phase 3: RAM + MAR (Week 4)
- Build RAM module
- Build MAR
- Can manually store and read data

### Phase 4: Program Counter + IR (Week 5)
- Build PC
- Build IR
- Manual fetch cycle testing

### Phase 5: Control Logic (Week 6-7)
- Program EEPROMs with microcode
- Wire control signals to all modules
- Test individual instructions

### Phase 6: Output + Programs (Week 8)
- Build output register + display
- Write test programs
- Debug full execution

### Phase 7: Assembler (Week 9+)
- Write assembler (Python/C) to convert assembly→machine code
- Program RAM with assembled code
- Run Fibonacci, prime calculator, etc.

## Success Criteria

1. Clock module generates stable, adjustable clock signal
2. Manual data entry to RAM via DIP switches works
3. Single-step through fetch+execute cycles
4. All 11 instructions execute correctly
5. Program runs automatically at full clock speed
6. Output display shows computed values
7. Fibonacci program runs and outputs correct sequence
8. Overflow/carry detection works (JC conditional jump)
