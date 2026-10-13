#include <stdio.h>
#include <stdlib.h>
#include "verilated.h"
#include "Vex1.h"

int main(int argc, char **argv) {
	Verilated::commandArgs(argc, argv);
	Vex1 *dut = new Vex1;
	
	dut->rst = 1;
	dut->clk = 0; dut->eval();
	dut->clk = 1; dut->eval();
	dut->rst = 0;


	for (int i=0; i<4; i++){
		dut->clk = 0;
		dut->eval();
		printf("cycle %d:    nb_a=%02x, nb_b=%02x | b_a=%02x    b_b=%02x\n", i, dut->nb_a, dut->nb_b, dut->b_a, dut->b_b);
		dut->clk = 1;
		dut->eval();
	}
	dut->final();
	delete dut;
	return 0;
}
