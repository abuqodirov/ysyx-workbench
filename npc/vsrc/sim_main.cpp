#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "verilated.h"
#include "Vtop.h"
#include <nvboard.h>

void nvboard_bind_all_pins(Vtop* top);

// Declare pointers globally so helper functions can access them
Vtop* top;
VerilatedContext* contextp;

// Simulates exactly one clock cycle (down, then up)
void single_cycle() {
    top->clk = 0; 
    top->eval();
    
    top->clk = 1; 
    top->eval();
}

// Holds the reset line HIGH for 'n' clock cycles, then drops it
void reset(int n) {
    top->rst = 1;
    while (n-- > 0) {
        single_cycle();
    }
    top->rst = 0;
}

int main(int argc, char** argv) {
    // Initialize Verilator environment
    contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    top = new Vtop{contextp};

    // Initialize NVBoard and bind pins
    nvboard_bind_all_pins(top);
    nvboard_init();

    // 1. Hold reset high for the first 10 cycles to initialize the hardware state cleanly
    reset(10);

    // 2. The main simulation loop
    while (!contextp->gotFinish()) {
        // Step forward by one clock cycle
        single_cycle();
        
        // Refresh the virtual board GUI
        nvboard_update(); 
    }

    // Clean up
    nvboard_quit();
    delete top;
    delete contextp;
    return 0;
}
