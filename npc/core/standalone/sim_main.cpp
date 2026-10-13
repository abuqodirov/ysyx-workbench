// Standalone NPC simulator (make SOC=0, used by ARCH=minirv-npc).
// Your CPU from core/vsrc/ plus NpcSim.v's SimpleBus memory -- no ysyxSoC, no
// NVBoard, no flash loader: the image goes straight to 0x8000_0000, which is
// also the reset PC. Far faster than the SoC build: use this one to find
// functional bugs, and the SoC build to measure performance.
//
// Environment knobs (all optional):
//   MAX_CYCLES=<n>       stop after n cycles, 0 = never      (default 2 billion)
//   WAVE_BEGIN/WAVE_END  VCD window in cycles from t=0       (make SOC=0 WAVE=1 builds only)
#include "verilated.h"
#include "VNpcSim.h"
#include "VNpcSim__Dpi.h"
#ifdef VCD_TRACE
#include "verilated_vcd_c.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PMEM_BASE 0x80000000u
#define PMEM_SIZE (128u * 1024 * 1024)

// The devices AM's runtime touches, at the SAME addresses as on ysyxSoC, so the
// one trm.c shared by minirv-npc and minirv-ysyxsoc works unchanged on both.
#define UART_BASE 0x10000000u   // UART16550: THR/DLL @+0, LCR @+3, LSR @+5
#define UART_SIZE 0x1000u
#define GPIO_BASE 0x20001000u   // LEDs / switches / 7-seg: writes ignored, reads 0
#define GPIO_SIZE 0x10u
#define BOOT_BASE 0x30000000u

static uint8_t  *pmem       = NULL;
static bool      running    = true;
static int       exit_code  = 0;     // 0 good trap, 1 bad trap, 2 cycle limit, 3 bad address
static uint64_t  inst_count = 0;
static uint8_t   uart_lcr   = 0;     // LCR bit 7 (DLAB) set -> offset 0 is the divisor, not THR

static void bad_access(const char *what, uint32_t addr) {
    fprintf(stderr, "\n[npc] %s of unmapped address 0x%08x -- stopping\n", what, addr);
    running   = false;
    exit_code = 3;
}

// DPI-C, called by SimpleBusMem once per accepted request. Reads return the
// whole aligned word; your CPU picks the byte lane itself (extracted_byte).
extern "C" int pmem_read(int raddr) {
    uint32_t a = (uint32_t)raddr & ~3u;
    if (a - PMEM_BASE < PMEM_SIZE) {
        uint32_t v;
        memcpy(&v, pmem + (a - PMEM_BASE), 4);
        return (int)v;
    }
		if (a == BOOT_BASE)     return 0x800002b7;
		if (a == BOOT_BASE + 4) return 0x00028067;
    if (a == UART_BASE + 4) return 0x60 << 8;   // LSR (+5) in byte lane 1: transmitter empty
    if (a - UART_BASE < UART_SIZE) return 0;
    if (a - GPIO_BASE < GPIO_SIZE) return 0;
    bad_access("read", (uint32_t)raddr);
    return 0;
}

extern "C" void pmem_write(int waddr, int wdata, char wmask) {
    uint32_t a = (uint32_t)waddr & ~3u;
    uint32_t d = (uint32_t)wdata;
    if (a - PMEM_BASE < PMEM_SIZE) {
        for (int i = 0; i < 4; i++)
            if (wmask & (1 << i)) pmem[a - PMEM_BASE + i] = (uint8_t)(d >> (8 * i));
        return;
    }
    if (a == UART_BASE) {                       // the word holding THR/DLL (+0) and LCR (+3)
        if (wmask & 0x8) uart_lcr = (uint8_t)(d >> 24);
        if ((wmask & 0x1) && !(uart_lcr & 0x80)) { putchar((int)(d & 0xff)); fflush(stdout); }
        return;
    }
    if (a - UART_BASE < UART_SIZE) return;      // other UART registers: ignore
    if (a - GPIO_BASE < GPIO_SIZE) return;      // LEDs / 7-seg: ignore
    bad_access("write", (uint32_t)waddr);
}

extern "C" void notify_retire(void) {
    inst_count++;
}

extern "C" void halt_sim(int a0_value) {
    if (a0_value == 0) {
        printf("\n\033[1;32mHIT GOOD TRAP (SUCCESS)\033[0m\n");
    } else {
        printf("\n\033[1;31mHIT BAD TRAP (Error Code: %d)\033[0m\n", a0_value);
        exit_code = 1;
    }
    running = false;
}

extern "C" void netlist_ebreak(void) {
    printf("\n\033[1;33mHIT EBREAK\033[0m (netlist: a0 not visible -- judge by the program's output above)\n");
    running = false;
}

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image.bin>\n", argv[0]);
        return 1;
    }

    pmem = (uint8_t *)calloc(PMEM_SIZE, 1);
    if (!pmem) {
        fprintf(stderr, "Error: out of memory\n");
        return 1;
    }
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Error: could not open %s\n", argv[1]);
        return 1;
    }
    size_t size = fread(pmem, 1, PMEM_SIZE, fp);
    fclose(fp);
    printf("Loaded %s (%zu bytes) at 0x%08x\n", argv[1], size, PMEM_BASE);

    VNpcSim *top = new VNpcSim;

#ifdef VCD_TRACE
    Verilated::traceEverOn(true);
    VerilatedVcdC *tfp = new VerilatedVcdC;
    top->trace(tfp, 99);
    tfp->open("wave.vcd");
    uint64_t vcd_time = 0;               // counts HALF cycles
    uint64_t wave_begin = 0, wave_end = 2000;
    if (const char *s = getenv("WAVE_BEGIN")) wave_begin = strtoull(s, NULL, 0);
    if (const char *s = getenv("WAVE_END"))   wave_end   = strtoull(s, NULL, 0);
  #define DUMP() do { if (vcd_time >= 2*wave_begin && vcd_time < 2*wave_end) \
                        tfp->dump(vcd_time); vcd_time++; } while (0)
#else
  #define DUMP() do { } while (0)
#endif

    auto single_cycle = [&]() {
        top->clock = 0; top->eval(); DUMP();
        top->clock = 1; top->eval(); DUMP();
    };

    top->reset = 1;
    for (int i = 0; i < 20; i++) single_cycle();
    top->reset = 0;

    uint64_t max_cycles = 2000000000ull;
    if (const char *s = getenv("MAX_CYCLES")) max_cycles = strtoull(s, NULL, 0);

    uint64_t cycle = 0;
    while (running && (max_cycles == 0 || cycle < max_cycles)) {
        single_cycle();
        cycle++;
    }
    if (running) {
        printf("\n[npc] stopped after %llu cycles (limit reached)\n", (unsigned long long)cycle);
        exit_code = 2;
    }

    printf("\n=== IPC (standalone NPC) ===\n");
    printf("  cycles       : %llu\n", (unsigned long long)cycle);
    printf("  instructions : %llu\n", (unsigned long long)inst_count);
    if (inst_count > 0) {
        printf("  IPC          : %.5f\n", (double)inst_count / (double)cycle);
        printf("  CPI          : %.2f\n", (double)cycle / (double)inst_count);
    }

    top->final();
#ifdef VCD_TRACE
    tfp->close();
    delete tfp;
#endif
    delete top;
    free(pmem);
    return exit_code;
}
