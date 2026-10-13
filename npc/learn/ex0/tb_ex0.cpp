#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "verilated.h"
#include "Vex0.h"


int main(int argc, char** argv){
	Verilated::commandArgs(argc, argv);
	Vex0 *dut = new Vex0;
	for (int i=0; i<2; i++){
		dut->a = i;
		dut->eval();
		printf("a=%d -> b=%d\n", dut->a, dut->b);
	}
	dut->final();
	delete dut;
	return 0;
}
