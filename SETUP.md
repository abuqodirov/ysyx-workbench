# SETUP: from zero to a working ysyx workbench

How to rebuild this environment on a fresh Linux machine: tools, the workbench, its subprojects, simulation, synthesis and the back-end flow.
Written for Kali Linux 2025.4 (Debian-based, x86_64) in VMware. Debian and Ubuntu should work the same way.

> **Paths.** `npc/Makefile` and the ECC config use absolute paths under `/home/kali/`.
> Either use the user name `kali` and the same folders as below, or edit the `YSYXSOC_HOME`,
> `NVBOARD_HOME` and `PDK_HOME` lines at the top of `npc/Makefile` and the paths in `ecc.toml` and `filelist.f`.

| What | Location |
|---|---|
| Workbench (this repo) | `~/ysyx-workbench` |
| oss-cad-suite | `~/oss/oss-cad-suite` |
| ICsprout55 PDK | `~/icsprout55-pdk` |
| ECC | `~/ecc/ecci` |
| ECOS Studio | `~/ecos/squashfs-root` (extracted AppImage), projects in `~/ecos/projects` |

---

## 1. System packages

```bash
sudo apt update
sudo apt install -y build-essential git python3 python-is-python3 \
  g++-riscv64-linux-gnu \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
  libreadline-dev llvm llvm-dev \
  time parallel python3-numpy
```

- `g++-riscv64-linux-gnu`: cross compiler for AM programs.
- SDL2 libraries: NVBoard.
- `libreadline-dev`, `llvm`: NEMU.
- `time`, `parallel`, `python3-numpy`: archbench's `run.sh`.

Do **not** install verilator, iverilog or yosys from apt. They come from oss-cad-suite.

## 2. oss-cad-suite (Verilator, Icarus Verilog, Yosys, GTKWave)

Download the Linux x64 archive from https://github.com/YosysHQ/oss-cad-suite-build/releases, then:

```bash
mkdir -p ~/oss && tar xzf ~/Downloads/oss-cad-suite-linux-x64-*.tgz -C ~/oss
echo 'export PATH="$PATH:$HOME/oss/oss-cad-suite/bin"' >> ~/.zshrc
```

Open a new terminal and check `verilator --version`, `iverilog -V`, `yosys -V`. The original machine used Verilator 5.051.

## 3. GitHub access

```bash
ssh-keygen -t ed25519 -C "<machine-name>"
cat ~/.ssh/id_ed25519.pub            # paste into GitHub: Settings > SSH and GPG keys > New SSH key
ssh -T git@github.com                # expect "Hi <user>!"
git config --global user.name "<name>"
git config --global user.email "<email>"
```

The subprojects are cloned over SSH (`git@github.com:...`), so the key is needed even for public repositories.

## 4. The workbench

```bash
git clone git@github.com:abuqodirov/ysyx-workbench.git ~/ysyx-workbench
cat >> ~/.zshrc <<'EOF'
export NEMU_HOME=$HOME/ysyx-workbench/nemu
export AM_HOME=$HOME/ysyx-workbench/abstract-machine
export NPC_HOME=$HOME/ysyx-workbench/npc
export NVBOARD_HOME=$HOME/ysyx-workbench/nvboard
EOF
```

Open a new terminal. Set `STUID` and `STUNAME` at the top of `~/ysyx-workbench/Makefile`: every NPC build is recorded on the `tracer-ysyx` branch with those values. A fresh clone starts a new tracer history; the original one lives in the backup bundle (section 12).

## 5. Subprojects, with this project's changes

`am-kernels`, `ysyxSoC`, `nvboard`, `archbench` and `fceux-am` are separate git repositories that the workbench ignores.
`my-patches/` holds the local changes to each, and `BASES.txt` lists the address and exact commit each patch applies to.

```bash
cd ~/ysyx-workbench
while read d url commit; do
  git clone "$url" "$d" && git -C "$d" checkout -q -b mine "$commit" \
    && { [ -s "my-patches/$d.patch" ] && git -C "$d" apply "../my-patches/$d.patch" || true; }
done < my-patches/BASES.txt
bash init.sh riscv-tests              # no local changes, so no patch
```

The `ysyxSoC` patch contains the GPIO controller (`perip/gpio/mygpio_top_apb.v`) and the CPU module name in `ready-to-run/minirv/ElaborateTop.v`.
To update the patches after changing a subproject: `bash my-patches/refresh.sh`.

## 6. Prebuilt klib (printf and friends)

The E stage uses a precompiled klib. Never edit `klib/src` and never run `make clean-all`: make would rebuild the library from stub sources, and `printf` would panic.

```bash
mkdir -p /tmp/klib-pre && cd /tmp/klib-pre
wget https://ysyx.oscc.cc/slides/resources/archive/klib-minirv-npc.tar.bz2 && tar xjf klib-minirv-npc.tar.bz2
mkdir -p $AM_HOME/klib/build && cd $AM_HOME/klib/build
cp "$(find /tmp/klib-pre -name klib-minirv-npc.a)" .
cp klib-minirv-npc.a klib-minirv-ysyxsoc.a && touch klib-minirv-npc.a klib-minirv-ysyxsoc.a
```

## 7. Smoke tests

```bash
# Standalone NPC (fast, 1-cycle memory): all PASS except `wrong`, which fails on purpose
make -C $NPC_HOME SOC=0
cd ~/ysyx-workbench/am-kernels/tests/cpu-tests && make ARCH=minirv-npc run

# NPC inside ysyxSoC with NVBoard: HIT GOOD TRAP
make -C $NPC_HOME
make ARCH=minirv-ysyxsoc ALL=dummy run

# Four-state simulation in Icarus Verilog: HIT GOOD TRAP
make -C $NPC_HOME icarus IMG=$HOME/ysyx-workbench/am-kernels/tests/cpu-tests/build/dummy-minirv-npc.bin
```

## 8. PDK and ECC (synthesis)

PDK (re-fetch if your copy predates 21 Aug 2026, when the simulation models were fixed for Verilator):

```bash
cd ~ && git clone -b unconfirmed git@github.com:openecos-projects/icsprout55-pdk
cd icsprout55-pdk && make unzip && cd patch && bash patch.sh
```

ECC v0.1.0-alpha.10: download `ecc-cli-linux-x86_64.tar.gz` from https://github.com/openecos-projects/ecc/releases and unpack it in `~/ecc` (on the original machine it unpacked into `~/ecc/ecci`). Then create the NPC project:

```bash
cd ~/ecc/ecci && ./ecc init npc
printf '%s\n' /home/kali/ysyx-workbench/npc/core/vsrc/top.v \
              /home/kali/ysyx-workbench/npc/core/vsrc/RegisterFile.v > npc/rtl/filelist.f
```

`~/ecc/ecci/npc/ecc.toml`:

```toml
[design]
name = "npc"
top = "top"
rtl = ["rtl/filelist.f"]
clock_port = "clock"
frequency_mhz = 100.0

[pdk]
name = "ics55"
root = "/home/kali/icsprout55-pdk"

[flow]
# preset: rtl2gds | rcx | harden | syn_sta
preset = "syn_sta"
run = "default"

[pdk.overrides]
libs = [
  "/home/kali/icsprout55-pdk/IP/STD_cell/ics55_LLSC_H7C_V1p10C100/ics55_LLSC_H7CR/liberty/ics55_LLSC_H7CR_typ_tt_1p2_25_nldm.lib",
  "/home/kali/icsprout55-pdk/IP/STD_cell/ics55_LLSC_H7C_V1p10C100/ics55_LLSC_H7CL/liberty/ics55_LLSC_H7CL_typ_tt_1p2_25_nldm.lib",
]
lefs = [
  "/home/kali/icsprout55-pdk/IP/STD_cell/ics55_LLSC_H7C_V1p10C100/ics55_LLSC_H7CR/lef/ics55_LLSC_H7CR_ecos.lef",
  "/home/kali/icsprout55-pdk/IP/STD_cell/ics55_LLSC_H7C_V1p10C100/ics55_LLSC_H7CL/lef/ics55_LLSC_H7CL_ecos.lef",
]
dont_use = [
  "*AO222*",
  "*2BB2*",
  "*AOI222*",
  "*AOI33*",
  "*OA222*",
  "*OAI222*",
  "*OAI33*",
  "*NOR4*",
]
```

**Use only the H7CR and H7CL libraries.** With H7CH added, synthesis picks H7CH cells, and ECOS Studio's default configuration silently drops every one of them, including all flip-flops, while still passing LVS.

```bash
cd ~/ecc/ecci && ./ecc run --overwrite --project npc
cat npc/runs/default/Synthesis_yosys/report/post_synthesis/qor_summary.rpt      # ~121 MHz, CELLA ~13,351
N=$(ls npc/runs/default/Synthesis_yosys/output/*_Synthesis.v.gz | grep -v _sim)
echo "LAT $(zcat $N | grep -c LAT)  H7CH $(zcat $N | grep -cE 'H7H[[:space:]]')  DFF $(zcat $N | grep -c DFFQX)"   # 0 / 0 / 1132
```

## 9. Netlist simulation

```bash
mkdir -p $NPC_HOME/core/netlist
zcat ~/ecc/ecci/npc/runs/default/Synthesis_yosys/output/*_Synthesis_sim.v.gz > $NPC_HOME/core/netlist/top_netlist.v
cd ~/ysyx-workbench/am-kernels/benchmarks/microbench && make ARCH=minirv-npc mainargs=test insert-arg

# Verilator on the gate-level netlist: MicroBench PASS in 36,161,236 cycles
make -C $NPC_HOME SOC=0 NETLIST=1 run IMG=$PWD/build/microbench-minirv-npc.bin

# Icarus on the gate-level netlist: HIT EBREAK
make -C $NPC_HOME icarus NETLIST=1 IMG=$HOME/ysyx-workbench/am-kernels/tests/cpu-tests/build/dummy-minirv-npc.bin
```

The netlist resets to `0x3000_0000` (the parameter is baked in); the standalone memory model serves a two-instruction boot stub there that jumps to `0x8000_0000`.

## 10. Back-end with ECOS Studio

- Download `ECOS-Studio_0.1.0-alpha.9_x86_64.AppImage` from https://github.com/openecos-projects/ecos-studio/releases/tag/v0.1.0-alpha.9 into `~/ecos`.
- Kali has no FUSE 2, so extract it once: `cd ~/ecos && ./ECOS-Studio_0.1.0-alpha.9_x86_64.AppImage --appimage-extract`. Start it with `~/ecos/squashfs-root/AppRun`.
- Run `mkdir -p ~/ecos/projects/npc` **before** Create Project, otherwise ECOS fails with `ENOENT ... realpath`.
- New workspace wizard: START STEP **Floorplan**; Import Verilog: the `*_Synthesis.v.gz` **without** `_sim`; PDK `/home/kali/icsprout55-pdk`, Default Config; Top Module `top`; Clock `clock`; Frequency max **40** (50 fails setup by about 1 ns).
- After the flow, confirm the flip-flops made it into the layout: `zcat ~/ecos/projects/npc/<workspace>/route_ecc/output/*.v.gz | grep -c DFFQX` must print about 1132.
- Expected checklist: 31 passed plus one warning, `config/macro_locations.txt` missing (no macros in this design).
- File > Export Signoff Package. If the menu item is greyed out, restart ECOS and reopen the workspace.
- Reference result: about 47 MHz post-layout, die 46,311 um^2, 31 % utilization.
- Tape-out submission: https://factory.openecos.com, template **OSOC**, upload `final/design/ysyx.def` and `ysyx.v` from the package (unzipped).

## 11. Day-to-day reference

Simulator knobs (environment variables):

| Variable | Effect | Default |
|---|---|---|
| `MAX_CYCLES` | stop after n cycles, 0 = never | 400M (SoC), 2B (standalone) |
| `CLK_RATIO` | SoC build: cpuClock runs k times faster than clock | 1 |
| `WAVE_BEGIN`, `WAVE_END` | VCD window in cycles; needs a `make WAVE=1` build | 0, 2000 |
| `NVBOARD_HOLD` | keep the NVBoard window open n seconds, -1 = until closed | 0 |
| `UART_TX_TRACE` | print UART TX level durations | off |

Simulator exit codes: 0 good trap, 1 bad trap, 2 cycle limit reached, 3 unmapped address (standalone).

| `npc/Makefile` target | Builds |
|---|---|
| `make` (`SOC=1`) | `obj_dir_soc/VSimTop`: NPC in ysyxSoC with NVBoard |
| `make SOC=0` | `obj_dir_npc/VNpcSim`: standalone NPC |
| `make SOC=0 NETLIST=1` | `obj_dir_netlist/VNpcSim`: synthesized netlist + PDK cells |
| `make icarus IMG=... [NETLIST=1]` | Icarus Verilog run |
| `... run IMG=<file.bin>` | build, then simulate |
| `... WAVE=1` | add VCD tracing (`rm -rf obj_dir_*` when switching it on or off) |

Gotchas:

- Building an image without `make run`: add `insert-arg` (`make ARCH=... mainargs=... insert-arg`), or `mainargs` stays a placeholder.
- `CPU_FREQ_KHZ` in `abstract-machine/am/src/riscv/npc/timer.c` must equal k x 25000 for performance runs on ysyxSoC.
- `marchid` is the `MARCHID_VAL` constant in `npc/core/vsrc/top.v`. Changing it means re-synthesizing and redoing the back-end.
- VMware: if the VM loses internet and `ping 192.168.245.2` fails, the NAT service on Windows has hung. In an administrator PowerShell: `Restart-Service "VMware NAT Service"`.

## 12. Backups that are not in this repo

- `ysyx-workbench-full.bundle`: the original workbench with its full history, including the `tracer-ysyx` branch. Restore with `git clone ysyx-workbench-full.bundle`.
- `ysyx_signoff_package.tar.gz`: the exported ECOS sign-off package.
- On the original machine, `~/ysyx-workbench` is the working copy and `~/ysyx-mine` is the published copy; after cloning this repo on a new machine, the clone is both. Commit on `main` and `git push`.
