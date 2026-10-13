#include <stdio.h>
#include "verilated.h"
#include "Vex5.h"

static void bits9(uint32_t v) {
  for (int i = 8; i >= 0; i--) putchar(((v >> i) & 1) ? '1' : '0');
}

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);
  Vex5 *dut = new Vex5;

  uint32_t insts[] = {
    0x00A50513, 0xFFF00593, 0x12345637, 0x00052683,
    0xFFC5C703, 0x00F12423, 0xFF088FA3, 0x00C58533,
    0x41248433, 0x000280E7, 0x00100073, 0x00554513,
    0x00000013, 0x00000000,
  };

  printf("inst        rd  rs1 rs2  imm         onehot\n");
  for (unsigned i = 0; i < sizeof(insts)/sizeof(insts[0]); i++) {
    dut->inst = insts[i];
    dut->eval();
    printf("0x%08x  %2u  %2u  %2u   0x%08x  ",
           insts[i], dut->rd, dut->rs1, dut->rs2, dut->imm);
    bits9(dut->onehot);
    putchar('\n');
  }

  dut->final();
  delete dut;
  return 0;
}
