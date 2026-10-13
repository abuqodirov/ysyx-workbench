#include <stdio.h>
#include <stdlib.h>
#include "verilated.h"
#include "Vex3.h"

int main(int argc, char** argv) {
	
	Verilated::commandArgs(argc, argv);
	Vex3 *dut = new Vex3;

	dut->rst = 1;
	dut->clk = 0; dut->eval();
	dut->clk = 1; dut->eval();
	dut->rst = 0;

	for (int i=0; i < 10; i++) {
		char state;
		dut->clk = 0; dut->eval();
		dut->go = 1;
		if (dut->busy == 1) {
			state = 'B';
		} else {
			state = 'A';
		}
		printf("The current state is: %c\n", state);
		dut->clk = 1; dut->eval();
	}

	dut->final();
	delete dut;
	
	return 0;
}
