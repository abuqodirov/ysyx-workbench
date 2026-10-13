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

    // Pure combinational loop driven by GUI switches
    while (!contextp->gotFinish()) {
        top->eval();
        nvboard_update();
    }

    nvboard_quit();
    delete top;
    delete contextp;
    return 0;
}
