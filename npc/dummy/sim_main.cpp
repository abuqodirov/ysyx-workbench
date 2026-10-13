#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "Vtop.h"
#include "verilated.h"
#include "svdpi.h"
#include "Vtop__Dpi.h"

// Define Memory Layout
#define PMEM_BASE 0x80000000
#define PMEM_SIZE (128 * 1024 * 1024) // 128 MB

bool simulation_running = true;
int sim_exit_code = 0;
uint8_t* pmem = NULL;

extern "C" void halt_sim(int a0_value) {
	if (a0_value == 0){
		printf("\n\033[1;32mHIT GOOD TRAP\033[0m\n");
		sim_exit_code = 0;	
	} else {
		printf("\n\033[1;31mHIT BAD TRAP\033[0m (Error Code: %d)\n", a0_value);
		sim_exit_code = 1;
	}
	simulation_running = false;
}

extern "C" void emu_init(const char* image_path);
extern "C" void emu_step();
extern "C" uint32_t emu_get_pc();
extern "C" uint32_t emu_get_reg(int idx);

// Function to load the .bin file into memory
void load_image(const char *image_path) {
    if (image_path == NULL) return;
    FILE *fp = fopen(image_path, "rb");
    if (!fp) {
        printf("Error: Could not open file %s\n", image_path);
        exit(1);
    }
    
    // Find out file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size > PMEM_SIZE) {
        printf("Error: Image size (%ld) exceeds memory size.\n", size);
        exit(1);
    }
    
    // Read the file directly into our memory array
    size_t ret = fread(pmem, size, 1, fp);
    assert(ret == 1);
    fclose(fp);
    printf("Successfully loaded %s (Size: %ld bytes)\n", image_path, size);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    
    // We now require the path to the .bin file as an argument
    if (argc < 2) {
        printf("Usage: %s <path_to_binary_file.bin>\n", argv[0]);
        return 1;
    }

    // Allocate 128MB of memory
    pmem = (uint8_t*)malloc(PMEM_SIZE);
    assert(pmem != NULL);
    
    // Load the binary into memory
    load_image(argv[1]);
		emu_init(argv[1]);

    Vtop* top = new Vtop;

    // Reset sequence
    top->rst = 1; top->clk = 0; top->eval();
    top->clk = 1; top->eval();
    top->rst = 0; top->clk = 0; top->eval();

    printf("--- Hardware minirv Execution Trace ---\n");

    int cycle_count = 0;
    
    while (simulation_running) {
        
        // 1. Fetch Instruction (with offset translation)
        uint32_t pc_offset = top->pc - PMEM_BASE;
        if (pc_offset < PMEM_SIZE) {
            top->inst = *((uint32_t*)(&pmem[pc_offset]));
        } else {
            top->inst = 0; // Out of bounds
        }
        
        top->eval();
        
        // 2. Handle Data Memory READS
        if (top->dmem_re) {
					if (top->dmem_addr == 0x10000000){
						top->dmem_rdata = 0;
					}
					else{
            uint32_t addr_offset = top->dmem_addr - PMEM_BASE;
            if (addr_offset < PMEM_SIZE) {
                top->dmem_rdata = *((uint32_t*)(&pmem[addr_offset]));
            } else {
                top->dmem_rdata = 0;
            }
            top->eval();
					}
        }
        
        printf("PC: 0x%08x | Inst: 0x%08x\n", top->pc, top->inst);

        top->clk = 1; 
        top->eval();

        // 3. Handle Data Memory WRITES
        if (top->dmem_we) {
					printf("[DEBUG] CPU wants to write to memory address 0x%08x\n", top->dmem_addr);
					if (top->dmem_addr == 0x10000000){
						fputc(top->dmem_wdata & 0xFF, stderr);
						fflush(stderr);
					}
					else {
            uint32_t addr_offset = top->dmem_addr - PMEM_BASE;
            if (addr_offset < PMEM_SIZE){
                if (top->dmem_wmask & 1) pmem[addr_offset]   = (top->dmem_wdata) & 0xFF;
                if (top->dmem_wmask & 2) pmem[addr_offset+1] = (top->dmem_wdata >> 8) & 0xFF;
                if (top->dmem_wmask & 4) pmem[addr_offset+2] = (top->dmem_wdata >> 16) & 0xFF;
                if (top->dmem_wmask & 8) pmem[addr_offset+3] = (top->dmem_wdata >> 24) & 0xFF;
            }
					}
        }

        top->clk = 0; 
        top->eval();
				emu_step();

				if (emu_get_pc() != top->pc) {
            printf("\n\033[1;31m[DiffTest] PC MISMATCH!\033[0m\n");
            printf("  Emulator PC: 0x%08x\n", emu_get_pc());
            printf("  NPC Hardware PC: 0x%08x\n", top->pc);
            sim_exit_code = 1;          // Tell Makefile it failed
            simulation_running = false; // Halt the simulation loop
        }
        
        cycle_count++;
    }

    printf("\nSimulation finished\n");
    printf("Final PC: 0x%08x\n", top->pc);
		printf("sim_exit_code: %d\n", sim_exit_code);

    free(pmem);
    delete top;
    return sim_exit_code;
}
