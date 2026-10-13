#include <stdio.h>
#include <stdlib.h>
#include "verilated.h"
#include "Vex2.h"

int main(int argc, char **argv) {
	Verilated::commandArgs(argc, argv);
	Vex2 *dut = new Vex2;

	dut->rst = 1;
	dut->clk = 0; dut->eval();
	dut->clk = 1; dut->eval();
	dut->rst = 0;

	for (int i=0; i < 10; i++) {
		dut->clk = 0; dut->eval();
		if (i < 3) {
			dut->en = 1;
		} else if (i > 2 && i < 7){
			dut->en = 0;
		} else {
			dut->en = 1;
		}
		dut->clk = 1; dut->eval();
		printf("Cycle=%d, count=%d\n", i, dut->count);
	}
	dut->final();
	delete dut;
	return 0;
}
