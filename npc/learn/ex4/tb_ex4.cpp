#include <stdio.h>
#include <stdlib.h>
#include "verilated.h"
#include "Vex4.h"
#include <assert.h>


static Vex4 *dut;

static void step() {
	dut->clk = 0; dut->eval();
	dut->clk = 1; dut->eval();
}


static bool do_read(uint32_t byte_addr, uint32_t *data, uint32_t *count) {
	dut->addr = byte_addr;
	dut->reqValid = 1;

	int n=0;
	do {
		step();
		n++;
		if (n>200) {
			dut->reqValid = 0; 
			return false;
		}
	} while (!dut->respValid);

	*data = dut->rdata;
	*count = n;
	dut->reqValid = 0;
	

  return true;
}


int main(int argc, char **argv) {
	Verilated::commandArgs(argc, argv);
	dut = new Vex4;

	dut->rst = 1; step();
	dut->rst = 0;
	
	int pass =0; int fail = 0;
	dut->reqValid = 0;
	for (int i=0; i < 20; i++) {
		step();
		if (dut->respValid) {printf("FAIL: respValid asserted with no request\n"); fail++; break; }
	}

	uint32_t addrs[] = { 0x000, 0x004, 0x028, 0x3FC};

	for (unsigned t = 0; t < sizeof(addrs)/sizeof(addrs[0]); t++) {
		uint32_t a = addrs[t];
		uint32_t expect = 0xA0000000u + (a >> 2);
		uint32_t data = 0, count = 0;

		if (!do_read(a, &data, &count)) {
			printf("addr 0x%03x -> TIMEOUT - respValid never arrived\n", a);
			fail++;
			continue;
		}

		step();
		bool pulsed = (dut->respValid == 0);
		bool ok = (data == expect) && pulsed;

		printf("addr 0x%03x -> 0x%08x (expect 0x%08x) %2u cycles pulse:%-3s %s\n", a, data, expect, count, pulsed ? "ok":"NO", ok ? "PASS" : "FAIL");
		ok ? pass++ : fail++;
	}

	printf("\n%d passed, %d failed\n", pass, fail);

	dut->final();
	delete dut;
	return (fail == 0) ? 0:1;
}
