#include <stdio.h>
#include <stdlib.h>
#include "verilated.h"
#include "Vex6.h"


static Vex6 *dut;

static void step() {
	dut->clk = 0; dut->eval();
	dut->clk = 1; dut->eval();
}

int main(int argc, char **argv) {
	Verilated::commandArgs(argc, argv);

	dut = new Vex6;


	dut->waddr = 0b00101;
	dut->wen = 1;
	dut->wdata = 0xDEADBEEF;
	dut->raddr1 = 0b00101;
	step();
	dut->wen = 1;
	dut->waddr = 0b01000;
	dut->wdata = 0x12345678;
	dut->raddr2 = 0b01000;
	step();

	printf("The value at the register 0x%2d is 0x%x\n", dut->raddr1, dut->rdata1);	
	printf("The value at the register 0x%2d is 0x%x\n", dut->raddr2, dut->rdata2);
	printf("-------------------------------------------------\n");
	
	dut->waddr = 0b00000;
	dut->wen = 1;
	dut->wdata = 0xABCDEFAB;
	dut->raddr1 = 0b00000;
	step();
	printf("The value at the register 0x%2d is 0x%x\n", dut->raddr1, dut->rdata1);	
	printf("-------------------------------------------------\n");
	dut->waddr = 5; dut->wdata = 0x11111111; dut->wen = 1;
	dut->raddr1 = 5;
	dut->eval();                              // settle, no edge
	printf("during: 0x%08x\n", dut->rdata1);
	step();
	printf("after:  0x%08x\n", dut->rdata1);


	dut->final();
	delete dut;
	return 0;
}
