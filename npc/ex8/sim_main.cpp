#include <stdio.h>
#include <stdlib.h>
#include "verilated.h"
#include "Vtop.h"
#include <nvboard.h>

void nvboard_bind_all_pins(Vtop* top);

Vtop* top;
VerilatedContext* contextp;

void single_cycle() {
    top->clk = 0; top->eval();
    top->clk = 1; top->eval();
}

int main(int argc, char** argv) {
    contextp = new VerilatedContext;
    contextp->commandArgs(argc, argv);
    top = new Vtop{contextp};

    nvboard_bind_all_pins(top);
    nvboard_init();

    top->clk = 0;
    top->rst = 0;

    while (!contextp->gotFinish()) {
        single_cycle();
        nvboard_update();
    }

    nvboard_quit();
    delete top;
    delete contextp;
    return 0;
}
