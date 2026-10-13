#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "verilated.h"
#include "Vtop.h"
#include <nvboard.h>

void nvboard_bind_all_pins(Vtop* top);

Vtop* top;
VerilatedContext* contextp;

void single_cycle() {
    top->clk = 0; 
    top->eval();
    top->clk = 1; 
    top->eval();
}

int main(int argc, char** argv) {
    contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    top = new Vtop{contextp};

    nvboard_bind_all_pins(top);
    nvboard_init();

    // Default signals
    top->clk = 0;
    top->clrn = 1; // Active-low clear released by default

    // Continuous execution loop
    while (!contextp->gotFinish()) {
        single_cycle();     // Tick clock to process PS/2 bits and update state machine
        nvboard_update();   // Process GUI events and capture keyboard input
    }

    nvboard_quit();
    delete top;
    delete contextp;
    return 0;
}
