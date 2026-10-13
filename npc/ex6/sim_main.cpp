#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "verilated.h"
#include "Vtop.h"
#include <nvboard.h>

void nvboard_bind_all_pins(Vtop* top);

int main(int argc, char** argv) {
    VerilatedContext* contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    Vtop* top = new Vtop{contextp};

    nvboard_bind_all_pins(top);
    nvboard_init();

    // Main loop: continuously update NVBoard state. 
    // The clock (top->clk) is now entirely controlled by SW0 in the GUI.
    while (!contextp->gotFinish()) {
        top->eval();
        nvboard_update();
    }

    nvboard_quit();
    delete top;
    delete contextp;
    return 0;
}
