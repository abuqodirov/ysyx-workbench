#include <nvboard.h>
#include <Vtop.h>
#include <stdint.h>
#include <stdio.h>

static TOP_NAME top;

void nvboard_bind_all_pins(TOP_NAME* top);

// ==========================================
// sEMU Implementation
// ==========================================

uint8_t PC = 0;
uint16_t R[4] = {0};
uint16_t M[256] = {
    0x0000, // LI  r0, 0
    0x1001, // LI  r1, 1
    0x2001, // LI  r2, 1
    0x300B, // LI  r3, 11
    0x4100, // ADD r0, r0, r1
    0xC000, // OUT r0
    0x5600, // ADD r1, r1, r2
    0x9FFD, // BNE r1, r3, -3
    0x8400  // BNE r0, r1, 0 (Halt)
};

void inst_cycle() {
    uint16_t inst = M[PC];
    
    // Decode: Extract fields using bitwise operations
    uint8_t opcode = (inst >> 14) & 0x03;
    uint8_t rd     = (inst >> 12) & 0x03;
    uint8_t rs1    = (inst >> 12) & 0x03; // For BNE/OUT
    uint8_t rs2    = (inst >> 10) & 0x03; // For BNE
    
    // Muxing rs1/rs2 for ADD instruction to match Verilog logic
    uint8_t rs1_add = (inst >> 10) & 0x03; 
    uint8_t rs2_add = (inst >> 8)  & 0x03; 
    
    uint16_t imm = inst & 0x0FFF;
    int8_t offset = (int8_t)(inst & 0x00FF); // Sign-extend 8-bit offset

    // Execute & Update PC
    if (opcode == 0) { // LI
        R[rd] = imm;
        PC++;
    } 
    else if (opcode == 1) { // ADD
        R[rd] = R[rs1_add] + R[rs2_add];
        PC++;
    } 
    else if (opcode == 2) { // BNE
        if (R[rs1] != R[rs2]) {
            PC = PC + offset;
        } else {
            PC++;
        }
    } 
    else if (opcode == 3) { // OUT
        // As noted in "image_8a1ae2.png": sEMU has no device abstraction. 
        // We simply update PC without writing to physical IO.
        PC++;
    }
}

// ==========================================
// Simulation & Difftest Harness
// ==========================================

int main() {
    nvboard_bind_all_pins(&top);
    nvboard_init();

    int last_clk = 0;
    
    printf("--- sCPU & sEMU Difftest Ready ---\n");

    while (1) {
        nvboard_update(); 
        top.eval();       

        // Detect a rising clock edge (Switch 1 flipped UP)
        if (top.clk == 1 && last_clk == 0) {
            
            if (top.rst == 1) {
                // Hardware Reset triggered via Switch 0
                printf("[RESET] Clearing sCPU and sEMU states.\n");
                PC = 0;
                for(int i=0; i<4; i++) R[i] = 0;
            } else {
                // 1. Run one instruction cycle in C (sEMU)
                uint8_t expected_pc = PC;
                inst_cycle(); 
                
                // 2. Fetch the hardware Program Counter (sCPU)
                uint8_t actual_pc = top.pc_out;
                
                // 3. Compare the behavior
                printf("Executed Instruction at PC: %d\n", expected_pc);
                
                if (PC != actual_pc) {
                    printf("\n[DIFFTEST FAILED!] PC Mismatch!\n");
                    printf("sEMU calculated next PC = %d\n", PC);
                    printf("sCPU calculated next PC = %d\n", actual_pc);
                    break; // Halt the simulation on error
                } else if (actual_pc == 8) {
                    printf("\n[SUCCESS] Halt instruction reached safely.\n");
                }
            }
        }
        
        last_clk = top.clk;
    }

    nvboard_quit();
    return 0;
}
