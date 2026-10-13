# minirv NPC · One Student One Chip

A multi-cycle RISC-V (minirv) processor core in Verilog, integrated into ysyxSoC, verified with Verilator and Icarus Verilog, synthesized with ECC and laid out with ECOS Studio on the ICsprout 55 nm PDK.

## What's here

| Path | Contents |
|---|---|
| `npc/core/vsrc/` | The core: `top.v` (FETCH/EXEC/MEM state machine, SimpleBus IFU and LSU, CSRs) and `RegisterFile.v` |
| `npc/core/soc/` | ysyxSoC + NVBoard testbench |
| `npc/core/standalone/` | Fast standalone testbench: CPU + SimpleBus memory model |
| `npc/core/icarus/` | Four-state simulation with a VPI memory model |
| `npc/Makefile` | `make SOC=1`, `make SOC=0`, `make SOC=0 NETLIST=1`, `make icarus` |
| `abstract-machine/` | Runtime with the minirv-ysyxsoc platform, UART16550 output, mcycle timer |
| `npc/ex*`, `npc/learn/` | Earlier exercises |

## Results

| | |
|---|---|
| ISA | minirv (8 instructions) + csrrs, ebreak |
| Flip-flops | 1,132 |
| Synthesis | 121 MHz (typical corner), 13,351 um^2 |
| Post-layout | ~47 MHz (signed off at 40 MHz, 31/31 checks), die 46,311 um^2 |
| Tests | cpu-tests and riscv-tests pass on ysyxSoC; microbench passes on RTL and gate-level netlist |

## Credits

Built on the One Student One Chip (ysyx) workbench. `nemu/`, `abstract-machine/` and the build scripts come from the NJU ProjectN / OSCPU templates.
