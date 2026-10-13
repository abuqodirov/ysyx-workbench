SOC  ?= 1
CORE  = core
.DEFAULT_GOAL := all

YSYXSOC_HOME = /home/kali/ysyx-workbench/ysyxSoC
NVBOARD_HOME = /home/kali/ysyx-workbench/nvboard
PDK_HOME     = /home/kali/icsprout55-pdk
include $(NVBOARD_HOME)/scripts/nvboard.mk

CORE_VSRCS = $(CORE)/vsrc/top.v $(CORE)/vsrc/RegisterFile.v
NETLIST_V  = $(CORE)/netlist/top_netlist.v
PDK_CELLS  = $(foreach c,H7CH H7CR H7CL,$(PDK_HOME)/IP/STD_cell/ics55_LLSC_H7C_V1p10C100/ics55_LLSC_$(c)/verilog/ics55_LLSC_$(c).v)

LINT_FLAGS = -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-PROCASSINIT -Wno-SYNCASYNCNET \
    -Wno-ASSIGNDLY -Wno-WIDTH -Wno-UNUSEDPARAM -Wno-UNDRIVEN -Wno-BLKSEQ \
    -Wno-DEFPARAM -Wno-PINCONNECTEMPTY -Wno-PINMISSING
VFLAGS = --cc --exe --build -j 0 -Wall $(LINT_FLAGS)

ifeq ($(SOC),1)
  TOP        = SimTop
  OBJ_DIR    = obj_dir_soc
  VSRCS      = $(CORE_VSRCS) $(YSYXSOC_HOME)/ready-to-run/minirv/ElaborateTop.v \
               $(shell find $(YSYXSOC_HOME)/perip -name "*.v" -o -name "*.sv")
  CSRCS      = $(CORE)/soc/sim_main.cpp auto_bind.cpp $(NVBOARD_ARCHIVE)
  VFLAGS    += --timescale "1ns/1ns" --no-timing -D__VERILOG__ -DPDK_BEHAV -D__UART_TO_CONSOLE__ \
               -I$(YSYXSOC_HOME)/perip/uart16550/rtl -I$(YSYXSOC_HOME)/perip/spi/rtl
  CXXFLAGS  += $(addprefix -I, $(INC_PATH)) $(shell sdl2-config --cflags)
  LDFLAGS   += $(shell sdl2-config --libs) -lSDL2_image -lSDL2_ttf
  BUILD_DEPS = auto_bind.cpp $(NVBOARD_ARCHIVE)
else
  TOP        = NpcSim
  CSRCS      = $(CORE)/standalone/sim_main.cpp
  BUILD_DEPS =
  ifeq ($(NETLIST),1)
    OBJ_DIR  = obj_dir_netlist
    VSRCS    = $(NETLIST_V) $(PDK_CELLS) $(CORE)/standalone/NpcSim.v
    VFLAGS  += --timescale "1ns/1ns" --no-timing -D__VERILATOR__ -Dfunctional -DNETLIST -Wno-fatal
  else
    OBJ_DIR  = obj_dir_npc
    VSRCS    = $(CORE_VSRCS) $(CORE)/standalone/NpcSim.v
  endif
endif

CXXFLAGS += -DTOP_NAME="\"V$(TOP)\""

ifeq ($(WAVE),1)
  VFLAGS   += --trace
  CXXFLAGS += -DVCD_TRACE
endif

all: build

# NVBoard pin bindings for SimTop, generated from the constraint file.
auto_bind.cpp: $(CORE)/soc/top.nxdc
	python3 $(NVBOARD_HOME)/scripts/auto_pin_bind.py $^ $@

build: $(BUILD_DEPS)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	verilator $(VFLAGS) --top-module $(TOP) --Mdir $(OBJ_DIR) $(VSRCS) $(CSRCS) \
		$(addprefix -CFLAGS , $(CXXFLAGS)) $(addprefix -LDFLAGS , $(LDFLAGS))

run: build
	./$(OBJ_DIR)/V$(TOP) $(IMG)

# E8: the standalone NPC in Icarus Verilog (four-state, memory over VPI).
#   make icarus IMG=<image.bin> [NETLIST=1] [ICARUS_ARGS="+MAX_CYCLES=<n> +WAVE"]
ICARUS_DIR = $(CORE)/icarus
ifeq ($(NETLIST),1)
  ICARUS_SRCS = -DNETLIST -Dfunctional $(NETLIST_V) $(PDK_CELLS)
else
  ICARUS_SRCS = $(CORE_VSRCS)
endif
icarus:
	cd $(ICARUS_DIR) && iverilog-vpi pmem_vpi.c
	iverilog -g2012 -s IcarusTop -o icarus.vvp $(ICARUS_DIR)/pmem_vpi.sft \
		$(ICARUS_DIR)/IcarusTop.v $(CORE)/standalone/NpcSim.v $(ICARUS_SRCS)
	vvp -M $(ICARUS_DIR) -mpmem_vpi icarus.vvp +IMG=$(IMG) $(ICARUS_ARGS)

.PHONY: all build run icarus
include ../Makefile
