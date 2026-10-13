#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint32_t PC = 0;

uint32_t R[16] = {0};

uint32_t M[256] = {
    // ---- Program ----
    [0x00 / 4] = 0x10000093, // addi x1,  x0, 256
    [0x04 / 4] = 0x12345137, // lui  x2,  0x12345
    [0x08 / 4] = 0x67810113, // addi x2,  x2, 0x678

    [0x0c / 4] = 0x0020a023, // sw   x2,  0(x1)
    [0x10 / 4] = 0x0000a183, // lw   x3,  0(x1)

    [0x14 / 4] = 0x0000c203, // lbu  x4,  0(x1)
    [0x18 / 4] = 0x0010c283, // lbu  x5,  1(x1)
    [0x1c / 4] = 0x0020c303, // lbu  x6,  2(x1)
    [0x20 / 4] = 0x0030c383, // lbu  x7,  3(x1)

    [0x24 / 4] = 0x00318433, // add  x8,  x3, x3

    // ---- Prepare bytes for SB ----
    [0x28 / 4] = 0x0ef00493, // addi x9,  x0, 0xef
    [0x2c / 4] = 0x0cd00513, // addi x10, x0, 0xcd
    [0x30 / 4] = 0x0ab00593, // addi x11, x0, 0xab
    [0x34 / 4] = 0x09000613, // addi x12, x0, 0x90

    [0x38 / 4] = 0x00c081a3, // sb   x12, 3(x1)
    [0x3c / 4] = 0x00b08123, // sb   x11, 2(x1)
    [0x40 / 4] = 0x00a080a3, // sb   x10, 1(x1)
    [0x44 / 4] = 0x00908023, // sb   x9,  0(x1)

    [0x48 / 4] = 0x0000a683, // lw   x13, 0(x1)

    // ---- Test JALR ----
    [0x4c / 4] = 0x06000713, // addi x14, x0, 0x60
    [0x50 / 4] = 0x000707e7, // jalr x15, 0(x14)

    // ---- Halt ----
    [0x54 / 4] = 0x05400067, // jalr x0, 84(x0) -> 0x54

    // ---- Function at 0x60 ----
    [0x60 / 4] = 0x00868733, // add x14, x13, x8
    [0x64 / 4] = 0x00078067  // jalr x0, 0(x15)
};


uint8_t load_byte(uint32_t addr)
{
    uint32_t word_index = addr / 4;
    uint32_t byte_index = addr % 4;

    return (M[word_index] >> (byte_index * 8)) & 0xFF;
}


void store_byte(uint32_t addr, uint8_t value)
{
    uint32_t word_index = addr / 4;
    uint32_t byte_index = addr % 4;
    uint32_t shift = byte_index * 8;

    M[word_index] &= ~(0xFFu << shift);
    M[word_index] |= ((uint32_t)value << shift);
}


uint32_t load_word(uint32_t addr)
{
    if (addr % 4 != 0) {
        printf("ERROR: misaligned LW address at 0x%08x\n", addr);
        exit(1);
    }

    return (uint32_t)load_byte(addr) |
           ((uint32_t)load_byte(addr + 1) << 8) |
           ((uint32_t)load_byte(addr + 2) << 16) |
           ((uint32_t)load_byte(addr + 3) << 24);
}


void store_word(uint32_t addr, uint32_t value)
{
    if (addr % 4 != 0) {
        printf("ERROR: misaligned SW at address 0x%08x\n", addr);
        exit(1);
    }

    store_byte(addr, value & 0xFF);
    store_byte(addr + 1, (value >> 8) & 0xFF);
    store_byte(addr + 2, (value >> 16) & 0xFF);
    store_byte(addr + 3, (value >> 24) & 0xFF);
}


void inst_cycle()
{
    uint32_t inst = M[PC / 4];

    uint32_t opcode = inst & 0x7F;
    uint32_t rd = (inst >> 7) & 0x1F;
    uint32_t funct3 = (inst >> 12) & 0x07;
    uint32_t rs1 = (inst >> 15) & 0x1F;
    uint32_t rs2 = (inst >> 20) & 0x1F;


    int32_t imm_i = (int32_t)(inst >> 20);

    if (imm_i & 0x800) {
        imm_i |= 0xFFFFF000;
    }

    int32_t imm_s = ((inst >> 25) << 5) | ((inst >> 7) & 0x1F);

    if (imm_s & 0x800) {
        imm_s |= 0xFFFFF000;
    }

    uint32_t imm_u = inst & 0xFFFFF000;
    uint32_t next_pc = PC + 4;


    // ADDI
    if (opcode == 0x13 && funct3 == 0x0) {
        if (rd != 0) {
            R[rd] = R[rs1] + imm_i;
        }

        printf("PC: 0x%02x | addi x%d, x%d, %d | x%d = 0x%08x\n", PC, rd, rs1, imm_i, rd, R[rd]);
    }


    // ADD
    else if (opcode == 0x33 && funct3 == 0x0 && ((inst >> 25) & 0x7F) == 0x00) {
        if (rd != 0) {
            R[rd] = R[rs1] + R[rs2];
        }

        printf("PC: 0x%02x | add x%d, x%d, x%d | x%d = 0x%08x\n", PC, rd, rs1, rs2, rd, R[rd]);
    }


    // LUI
    else if (opcode == 0x37) {
        if (rd != 0) {
            R[rd] = imm_u;
        }

        printf("PC: 0x%02x | lui x%d, 0x%05x | x%d = 0x%08x\n", PC, rd, imm_u >> 12, rd, R[rd]);
    }


    // LOAD
    else if (opcode == 0x03) {
        uint32_t addr = R[rs1] + imm_i;

        // LW
        if (funct3 == 0x2) {
            uint32_t value = load_word(addr);

            if (rd != 0) {
                R[rd] = value;
            }

            printf("PC: 0x%02x | lw x%d, %d(x%d) | addr=0x%08x | x%d=0x%08x\n", PC, rd, imm_i, rs1, addr, rd, R[rd]);
        }

        // LBU
        else if (funct3 == 0x4) {
            uint8_t value = load_byte(addr);

            if (rd != 0) {
                R[rd] = value;
            }

            printf("PC: 0x%02x | lbu x%d, %d(x%d) | addr=0x%08x | x%d=0x%08x\n", PC, rd, imm_i, rs1, addr, rd, R[rd]);
        }

        else {
            printf("ERROR: unknown LOAD funct3 0x%x at PC 0x%x\n", funct3, PC);
            exit(1);
        }
    }


    // STORE
    else if (opcode == 0x23) {
        uint32_t addr = R[rs1] + imm_s;

        // SB
        if (funct3 == 0x0) {
            store_byte(addr, R[rs2] & 0xFF);

            printf("PC: 0x%02x | sb x%d, %d(x%d) | addr=0x%08x | byte=0x%02x\n", PC, rs2, imm_s, rs1, addr, R[rs2] & 0xFF);
        }

        // SW
        else if (funct3 == 0x2) {
            store_word(addr, R[rs2]);

            printf("PC: 0x%02x | sw x%d, %d(x%d) | addr=0x%08x | value=0x%08x\n", PC, rs2, imm_s, rs1, addr, R[rs2]);
        }

        else {
            printf("ERROR: unknown STORE funct3 0x%x at PC 0x%x\n", funct3, PC);
            exit(1);
        }
    }


    // JALR
    else if (opcode == 0x67 && funct3 == 0x0) {
        uint32_t target = (R[rs1] + imm_i) & ~1u;

        if (rd != 0) {
            R[rd] = PC + 4;
        }

        printf("PC: 0x%02x | jalr x%d, %d(x%d) -> Target: 0x%02x\n", PC, rd, imm_i, rs1, target);

        next_pc = target;
    }


    else {
        printf("Unknown opcode 0x%x at PC 0x%x\n", opcode, PC);
        exit(1);
    }

    PC = next_pc;
    R[0] = 0;
}


int main()
{
    printf("--------- miniRV Execution Trace ----------\n");

    for (int i = 0; i < 28; i++) inst_cycle();

    printf("\nFinal register state:\n");
    for (int i = 0; i < 16; i++) printf("x%-2d = 0x%08x\n", i, R[i]);

    printf("\nFinal memory at 0x100: 0x%08x\n", load_word(0x100));
    printf("Bytes: %02x %02x %02x %02x\n", load_byte(0x100), load_byte(0x101), load_byte(0x102), load_byte(0x103));

    return 0;
}
