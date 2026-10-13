# ~/ysyx-workbench/npc/learn/Makefile
# Standalone sandbox. Knows nothing about ysyxSoC, NVBoard or simplebus/.
#
# Layout:  learn/exN/exN.v  +  learn/exN/tb_exN.cpp

TOP ?= ex1
DIR := $(TOP)
OBJ := $(DIR)/obj_dir
BIN := $(OBJ)/V$(TOP)

VFLAGS := --cc --exe --build -j 0 \
          --top-module $(TOP) --Mdir $(OBJ) \
          -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL

ifdef LATENCY
	VFLAGS += -GLATENCY=$(LATENCY)
endif

SRCS := $(DIR)/$(TOP).v $(DIR)/tb_$(TOP).cpp

run: build
	@echo "--- $(TOP) ---"
	@./$(BIN)

build:
	@test -f $(DIR)/$(TOP).v      || { echo "missing $(DIR)/$(TOP).v"; exit 1; }
	@test -f $(DIR)/tb_$(TOP).cpp || { echo "missing $(DIR)/tb_$(TOP).cpp"; exit 1; }
	verilator $(VFLAGS) $(SRCS)

list:
	@ls -d ex*/ 2>/dev/null | sed 's#/##' || echo "(no exercises yet)"

new:
	@mkdir -p $(DIR)
	@test -f $(DIR)/$(TOP).v || \
	  printf 'module $(TOP)(\n  input clk\n);\n\nendmodule\n' > $(DIR)/$(TOP).v
	@test -f $(DIR)/tb_$(TOP).cpp || \
	  printf '#include "V$(TOP).h"\n#include <stdio.h>\n\nint main() {\n  V$(TOP) dut;\n  dut.eval();\n  return 0;\n}\n' > $(DIR)/tb_$(TOP).cpp
	@echo "created $(DIR)/"

clean:
	rm -rf */obj_dir

.PHONY: run build list clean new
