#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PMEM_BASE 0x80000000
#define PMEM_SIZE (128 * 1024 * 1024) // 128 MB

static uint32_t emu_PC = PMEM_BASE;
static uint32_t emu_R[16] = {0}; // The [16] is mandatory here
static uint8_t* emu_pmem = NULL;

extern "C" void emu_init(const char* image_path) {
    if (emu_pmem == NULL) {
        emu_pmem = (uint8_t*)malloc(PMEM_SIZE);
    }
    memset(emu_pmem, 0, PMEM_SIZE);
    memset(emu_R, 0, sizeof(emu_R));
    emu_PC = PMEM_BASE;

    if (image_path != NULL) {
        FILE *fp = fopen(image_path, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            fread(emu_pmem, size, 1, fp);
            fclose(fp);
        }
    }
}

extern "C" uint32_t emu_load_mem(uint32_t addr) {
    uint32_t offset = addr - PMEM_BASE;
    if (offset + 3 < PMEM_SIZE) {
        return *((uint32_t*)(&emu_pmem[offset]));
    }
    return 0;
}

extern "C" uint8_t emu_load_byte(uint32_t addr) {
    uint32_t offset = addr - PMEM_BASE;
    if (offset < PMEM_SIZE) {
        return emu_pmem[offset];
    }
    return 0;
}

extern "C" void emu_store_byte(uint32_t addr, uint8_t value) {
    uint32_t offset = addr - PMEM_BASE;
    if (offset < PMEM_SIZE) {
        emu_pmem[offset] = value;
    }
}

extern "C" void emu_store_word(uint32_t addr, uint32_t value) {
    uint32_t offset = addr - PMEM_BASE;
    if (offset + 3 < PMEM_SIZE) {
        *((uint32_t*)(&emu_pmem[offset])) = value;
    }
}

extern "C" void emu_step() {
    uint32_t inst = emu_load_mem(emu_PC);

    uint32_t opcode = inst & 0x7F;
    uint32_t rd = (inst >> 7) & 0x1F;
    uint32_t funct3 = (inst >> 12) & 0x07;
    uint32_t rs1 = (inst >> 15) & 0x1F;
    uint32_t rs2 = (inst >> 20) & 0x1F;

    int32_t imm_i = (int32_t)(inst >> 20);
    if (imm_i & 0x800) imm_i |= 0xFFFFF000;

    int32_t imm_s = ((inst >> 25) << 5) | ((inst >> 7) & 0x1F);
    if (imm_s & 0x800) imm_s |= 0xFFFFF000;

    uint32_t imm_u = inst & 0xFFFFF000;
    uint32_t next_pc = emu_PC + 4;

    // ADDI
    if (opcode == 0x13 && funct3 == 0x0) {
        if (rd != 0) emu_R[rd] = emu_R[rs1] + imm_i;
    }
    // ADD / SUB 
    else if (opcode == 0x33 && funct3 == 0x0) {
        uint32_t funct7 = (inst >> 25) & 0x7F;
        if (rd != 0) {
            if (funct7 == 0x20) emu_R[rd] = emu_R[rs1] - emu_R[rs2]; // SUB
            else                emu_R[rd] = emu_R[rs1] + emu_R[rs2]; // ADD
        }
    }
    // LUI
    else if (opcode == 0x37) {
        if (rd != 0) emu_R[rd] = imm_u;
    }
    // LOAD (LW, LBU)
    else if (opcode == 0x03) {
        uint32_t addr = emu_R[rs1] + imm_i;
        if (funct3 == 0x2) { 
            uint32_t value = emu_load_mem(addr);
            if (rd != 0) emu_R[rd] = value;
        } else if (funct3 == 0x4) { 
            uint8_t value = emu_load_byte(addr);
            if (rd != 0) emu_R[rd] = value;
        }
    }
    // STORE (SB, SW)
    else if (opcode == 0x23) {
        uint32_t addr = emu_R[rs1] + imm_s;
        if (funct3 == 0x0) { 
            emu_store_byte(addr, emu_R[rs2] & 0xFF);
        } else if (funct3 == 0x2) { 
            emu_store_word(addr, emu_R[rs2]);
        }
    }
    // JALR
    else if (opcode == 0x67 && funct3 == 0x0) {
        uint32_t target = (emu_R[rs1] + imm_i) & ~1u;
        if (rd != 0) emu_R[rd] = emu_PC + 4;
        next_pc = target;
    }
    // EBREAK 
    else if (inst == 0x00100073) {
    }

    emu_PC = next_pc;
    emu_R[0] = 0; 
}

extern "C" uint32_t emu_get_pc() {
    return emu_PC;
}

extern "C" uint32_t emu_get_reg(int idx) {
    if (idx >= 0 && idx < 16) return emu_R[idx];
    return 0;
}
