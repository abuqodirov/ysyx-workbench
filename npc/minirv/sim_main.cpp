#include <stdio.h>
#include <stdint.h>
#include "Vtop.h"
#include "verilated.h"
#include "svdpi.h"
#include "Vtop__Dpi.h"

bool simulation_running = true;

extern "C" void halt_sim() {
    printf("\n[DPI-C] EBREAK instruction detected! Halting simulation.\n");
    simulation_running = false;
}

uint8_t data_memory[1024] = {0};

uint32_t pmem_read(uint32_t pc) {
    static uint32_t M[256] = {0};
    static bool initialized = false;
    
    // C++ friendly way to initialize specific array indices
    if (!initialized) {
        // ---- Program ----
        M[0x00 / 4] = 0x10000093; // addi x1,  x0, 256
        M[0x04 / 4] = 0x12345137; // lui  x2,  0x12345
        M[0x08 / 4] = 0x67810113; // addi x2,  x2, 0x678

        M[0x0c / 4] = 0x0020a023; // sw   x2,  0(x1)
        M[0x10 / 4] = 0x0000a183; // lw   x3,  0(x1)

        M[0x14 / 4] = 0x0000c203; // lbu  x4,  0(x1)
        M[0x18 / 4] = 0x0010c283; // lbu  x5,  1(x1)
        M[0x1c / 4] = 0x0020c303; // lbu  x6,  2(x1)
        M[0x20 / 4] = 0x0030c383; // lbu  x7,  3(x1)

        M[0x24 / 4] = 0x00318433; // add  x8,  x3, x3

        // ---- Prepare bytes for SB ----
        M[0x28 / 4] = 0x0ef00493; // addi x9,  x0, 0xef
        M[0x2c / 4] = 0x0cd00513; // addi x10, x0, 0xcd
        M[0x30 / 4] = 0x0ab00593; // addi x11, x0, 0xab
        M[0x34 / 4] = 0x09000613; // addi x12, x0, 0x90

        M[0x38 / 4] = 0x00c081a3; // sb   x12, 3(x1)
        M[0x3c / 4] = 0x00b08123; // sb   x11, 2(x1)
        M[0x40 / 4] = 0x00a080a3; // sb   x10, 1(x1)
        M[0x44 / 4] = 0x00908023; // sb   x9,  0(x1)

        M[0x48 / 4] = 0x0000a683; // lw   x13, 0(x1)

        // ---- Test JALR ----
        M[0x4c / 4] = 0x06000713; // addi x14, x0, 0x60
        M[0x50 / 4] = 0x000707e7; // jalr x15, 0(x14)

        // ---- Halt ----
        M[0x54 / 4] = 0x00100073; // ebreak (Replaced jump to self)

        // ---- Function at 0x60 ----
        M[0x60 / 4] = 0x00868733; // add x14, x13, x8
        M[0x64 / 4] = 0x00078067; // jalr x0, 0(x15)
        
        initialized = true;
    }
    
    // Boundary protection for up to 1024 bytes (256 words)
    if (pc >= 1024) return 0;
    return M[pc / 4];
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vtop* top = new Vtop;

    // Reset sequence
    top->rst = 1; top->clk = 0; top->eval();
    top->clk = 1; top->eval();
    top->rst = 0; top->clk = 0; top->eval();

    printf("--- Hardware minirv Execution Trace ---\n");

    // Loop until we hit the halt address (0x0c) or a timeout to prevent infinite loops
    int max_cycles = 100;
    int cycle_count = 0;
    
    while (simulation_running && cycle_count < max_cycles) {
        
        // 1. Fetch Instruction using the current PC
        top->inst = pmem_read(top->pc);
        
        // 2. Evaluate combinational logic (decoding and routing)
        top->eval();
				if (top->dmem_re){
					if (top->dmem_addr < sizeof(data_memory)) {
						top->dmem_rdata = *((uint32_t*)(&data_memory[top->dmem_addr]));
					} else {
						top->dmem_rdata = 0;
					}

					top->eval();
				}
        
        printf("PC: 0x%02x | Inst: 0x%08x\n", top->pc, top->inst);
        if (top->dmem_we) {
            printf("  -> Mem Write: Addr 0x%04x | Data 0x%08x | Mask 0x%x\n", 
                   top->dmem_addr, top->dmem_wdata, top->dmem_wmask);
        }
        if (top->dmem_re) {
            printf("  <- Mem Read: Addr 0x%04x | Data 0x%08x\n", 
                   top->dmem_addr, top->dmem_rdata);
        }

        top->clk = 1; top->eval();

				if (top->dmem_we) {
					uint32_t addr = top->dmem_addr;

					if (addr < sizeof(data_memory)){
						if (top->dmem_wmask & 1) data_memory[addr]   = (top->dmem_wdata) & 0xFF;
            if (top->dmem_wmask & 2) data_memory[addr+1] = (top->dmem_wdata >> 8) & 0xFF;
            if (top->dmem_wmask & 4) data_memory[addr+2] = (top->dmem_wdata >> 16) & 0xFF;
            if (top->dmem_wmask & 8) data_memory[addr+3] = (top->dmem_wdata >> 24) & 0xFF;
					}
				}

        top->clk = 0; top->eval();
        
        cycle_count++;
    }

    // Print the final state upon hitting the halt instruction
    printf("\nHalted at PC: 0x%02x\n", top->pc);
		uint32_t final_mem_val = *((uint32_t*)(&data_memory[256]));
    printf("Simulation finished. Memory at 0x100 = 0x%08x\n", final_mem_val);

    delete top;
    return 0;
}
