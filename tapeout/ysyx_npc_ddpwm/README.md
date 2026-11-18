# ysyx_npc_ddpwm: tape-out files

A RISC-V core (minirv) with a dyadic digital PWM (DDPWM) engine, implemented as one core slot of the OSOC-DE shared SoC and submitted to the ECOS55-2610 shuttle (ICsprout 55 nm CMOS) on 19 October 2026.

## Contents

| Path | Description |
|---|---|
| `gds/ysyx_npc_ddpwm.gds` | Final layout (GDSII) |
| `constraints/ysyx_npc_ddpwm.sdc` | Timing constraints: clock `clock`, 40 MHz |
| `layout/ysyx_npc_ddpwm.def.gz` | Final layout (DEF), as uploaded to ECOS Factory |
| `netlist/ysyx_npc_ddpwm.v.gz` | Post-layout gate-level netlist, as uploaded to ECOS Factory |
| `netlist/ysyx_npc_ddpwm_synthesis.v.gz` | Synthesized gate-level netlist, the input of the layout flow |
| `ecos/parameters.json`, `ecos/config/` | ECOS Studio settings of the run (floorplan, placement, CTS, routing, sign-off) |

**Source code:** the RTL is in [`npc/core/vsrc-ddpwm/`](../../npc/core/vsrc-ddpwm/):

| File | Contents |
|---|---|
| `top.v` | The RISC-V core (top module `top`) |
| `DdpwmEngine.v` | The DDPWM engine |
| `RegisterFile.v` | The register file |

## Flow

1. **Synthesis:** RTL → gate-level netlist with ECC (Yosys), on the ICsprout 55 nm H7CR and H7CL standard-cell libraries.
2. **Layout:** ECOS Studio, starting at Floorplan. Steps: placement, fan-out repair, clock-tree synthesis, routing, fillers, DRC, LVS, RC extraction and STA.
   - Design `ysyx_npc_ddpwm`, top module `top`, clock `clock`
   - 40 MHz, core utilization 0.5, core margin 2 µm
3. **Sign-off checklist:** 31 of 32 checks passed, 0 blocked. The one warning is the optional `macro_locations` file, which does not apply because the design has no macros.

## Key figures

| | |
|---|---|
| Die | 214.6 × 215.4 µm (0.046 mm²) |
| Standard cells | 7,453 (1,560 flip-flops, 32 clock-gating cells) |
| Transistors | ≈ 92,000 in logic cells |
| Timing | Setup slack +8.1 ns at 40 MHz |

## Fingerprints (MD5)

| File | MD5 |
|---|---|
| `ysyx_npc_ddpwm.gds` | `cc0bec51553a8a619b4c80492f6c0ddb` |
| `ysyx_npc_ddpwm.def` (unpacked; the uploaded file) | `2a263e4b45671f5e2850012452ca2ca2` |
| `ysyx_npc_ddpwm.v` (unpacked; the uploaded file) | `8132d8b798dbba3559aad0ade9fa203f` |
| `ysyx_npc_ddpwm.sdc` | `da0550ec7a9533677f2984d4f069b03c` |
| `ysyx_npc_ddpwm_synthesis.v` (unpacked) | `f2ab7decaf032ae735ba0b6a1aa35c10` |
