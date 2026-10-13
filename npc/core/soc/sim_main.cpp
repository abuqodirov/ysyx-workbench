#include "verilated.h"
#include "svdpi.h"
#include "VSimTop__Dpi.h"
#include "VSimTop.h"
#ifdef VCD_TRACE
#include "verilated_vcd_c.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <nvboard.h>
#include <time.h>
#include <unistd.h>

// Environment knobs (all optional):
//   CLK_RATIO=<k>       cpuClock = k x clock                         (default 1)
//   MAX_CYCLES=<n>      stop after n cpuClock cycles, 0 = never      (default 400M)
//   WAVE_BEGIN/WAVE_END VCD window in cpuClock cycles from t=0       (make WAVE=1 builds only)
//   NVBOARD_HOLD=<s>    keep NVBoard open s seconds at the end, -1 = until closed
//   UART_TX_TRACE=1     print individual UART TX level durations

// 16MB == the whole 24-bit address space an SPI flash command can name.
#define FLASH_SIZE (16 * 1024 * 1024)

static bool      simulation_running = true;
static int       sim_exit_code      = 0;   // main()'s return value = what make sees
static uint8_t  *flash_mem          = NULL;
static long      flash_image_size   = 0;
static int       flash_trace_left   = 8;   // dump the first few reads, then go quiet

// --- two clocks: cpuClock (NPC) runs K times faster than clock (SoC) --------
static unsigned  clk_ratio = 1;   // K, set from env CLK_RATIO (default 1)
static unsigned  half_cnt  = 0;   // cpuClock half-periods since `clock` last toggled
static uint8_t   soc_clk   = 1;   // current level of `clock` (starts high, see main)

void nvboard_bind_all_pins(VSimTop *top);

// Internal NVBoard symbol: defined in nvboard/src/nvboard.cpp but only declared
// in nvboard/include/nvboard.h, which is not on our include path (nvboard.mk
// exports usr/include instead). Plain C++ linkage, so this matches at link time.
void set_redraw();

// --- UART TX bit-period measurement ----------------------------------------
// The shortest run of a constant level on the TX line IS one bit period. It is
// sampled once per `clock` (SoC) cycle -- the same unit NVBoard's divisor
// counts in -- so it must read 208 for every K.
// Set UART_TX_TRACE=1 to also print each individual level duration.
static uint8_t   tx_prev       = 1;
static uint64_t  tx_run        = 0;
static uint64_t  tx_min        = UINT64_MAX;
static int       tx_trace_left = 20;
static bool      tx_trace_on   = false;

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
}

// Keep the NVBoard window alive after the simulation ends.
//   NVBOARD_HOLD=<n>   hold for n seconds
//   NVBOARD_HOLD=-1    hold until you close the window / Ctrl-C
//   unset or 0         quit immediately (default, keeps batch runs working)
static void nvboard_hold(void) {
    const char *env = getenv("NVBOARD_HOLD");
    int secs = env ? atoi(env) : 0;
    if (secs == 0) return;

    if (secs < 0) printf("[sim] NVBoard held open - close the window or Ctrl-C to exit.\n");
    else          printf("[sim] holding NVBoard window for %d s...\n", secs);

    // NVBoard's frame throttle (nvboard.cpp) adapts a "calls per frame" counter
    // to however often nvboard_update() is called. During the simulation that is
    // once per simulated cycle, so the counter is left calibrated for a very
    // high call rate -- around 16000 calls before it even checks the clock.
    // Calling it at a leisurely 500Hz here would mean ~30 seconds of a frozen,
    // unrepainted window. So keep the call rate high and sleep only between
    // batches; the counter drains in well under a frame and rendering resumes.
    uint64_t start = now_us();
    while (secs < 0 || now_us() - start < (uint64_t)secs * 1000000ull) {
        set_redraw();       // force a present even if nothing new was drawn
        for (int i = 0; i < 2000; i++) nvboard_update();
        usleep(1000);
    }
}

// ---------------------------------------------------------------------------
// DPI-C: called by ysyxSoC/perip/flash/flash.v once per 4-byte flash read.
// `addr` is the 24-bit offset the SPI controller drove onto the wire, NOT a
// 0x3000_0000-based CPU address -- the SPI controller already stripped the base.
// ---------------------------------------------------------------------------
extern "C" void flash_read(int32_t addr, int32_t *data) {
    uint32_t offset = (uint32_t)addr & 0x00ffffffu;

    uint32_t val;
    if (offset + 4 <= FLASH_SIZE) {
        memcpy(&val, flash_mem + offset, 4);
    } else {
        val = 0;
        fprintf(stderr, "[flash] WARNING: read past end of flash, addr=0x%06x\n", offset);
    }

#ifdef FLASH_BSWAP
    val = __builtin_bswap32(val);
#endif

    *data = (int32_t)val;

    if (flash_trace_left > 0) {
        flash_trace_left--;
        fprintf(stderr, "[flash] addr=0x%06x -> 0x%08x\n", offset, val);
    }
}

static uint64_t inst_count = 0;

extern "C" void notify_retire(void) {
    inst_count++;
}

extern "C" void halt_sim(int a0_value) {
    if (a0_value == 0) {
        printf("\n\033[1;32mHIT GOOD TRAP (SUCCESS)\033[0m\n");
    } else {
        printf("\n\033[1;31mHIT BAD TRAP (Error Code: %d)\033[0m\n", a0_value);
        sim_exit_code = 1;
    }
    simulation_running = false;
}

static void load_flash(const char *image_path) {
    FILE *fp = fopen(image_path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: could not open flash image %s\n", image_path);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    flash_image_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (flash_image_size <= 0 || flash_image_size > FLASH_SIZE) {
        fprintf(stderr, "Error: bad flash image size %ld (limit %d)\n",
                flash_image_size, FLASH_SIZE);
        exit(1);
    }

    size_t got = fread(flash_mem, 1, (size_t)flash_image_size, fp);
    if (got != (size_t)flash_image_size) {
        fprintf(stderr, "Error: short read on %s (%zu of %ld bytes)\n",
                image_path, got, flash_image_size);
        exit(1);
    }
    fclose(fp);

    uint32_t first;
    memcpy(&first, flash_mem, 4);
    printf("Loaded flash image %s (%ld bytes); first word in file = 0x%08x\n",
           image_path, flash_image_size, first);
}

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <hello-minirv-ysyxsoc.bin>\n", argv[0]);
        return 1;
    }

    // calloc, not malloc: everything outside the image must read as zero, not
    // as heap garbage that the CPU might fetch and try to execute.
    flash_mem = (uint8_t *)calloc(FLASH_SIZE, 1);
    assert(flash_mem != NULL);
    load_flash(argv[1]);

    VSimTop *top = new VSimTop;
    nvboard_bind_all_pins(top);   // must come BEFORE init: the UART
    nvboard_init();               // constructor caches the TX pin pointer

#ifdef VCD_TRACE
    Verilated::traceEverOn(true);
    VerilatedVcdC *tfp = new VerilatedVcdC;
    top->trace(tfp, 99);                 // 99 = every level, down into your NPC
    tfp->open("wave.vcd");
    // Only record cpuClock cycles [WAVE_BEGIN, WAVE_END), counted from time 0
    // (reset included). Default: the first 2000 cycles.
    uint64_t vcd_time = 0;               // counts cpuClock HALF periods
    uint64_t wave_begin = 0, wave_end = 2000;
    if (const char *s = getenv("WAVE_BEGIN")) wave_begin = strtoull(s, NULL, 0);
    if (const char *s = getenv("WAVE_END"))   wave_end   = strtoull(s, NULL, 0);
  #define DUMP() do { if (vcd_time >= 2*wave_begin && vcd_time < 2*wave_end) \
                        tfp->dump(vcd_time); vcd_time++; } while (0)
#else
  #define DUMP() do { } while (0)
#endif

    if (const char *s = getenv("CLK_RATIO")) {
        int k = atoi(s);
        clk_ratio = (k < 1) ? 1 : (unsigned)k;
    }

    // One cpuClock cycle = two half-periods (low, then high). `clock` toggles
    // every K half-periods, so its full period is 2K half-periods = K cpuClock
    // cycles. Starting it HIGH makes every rising edge of `clock` land on a
    // rising edge of cpuClock, for odd K too:
    //   K=3   cpuClock 0 1 0 1 0 1 0 1 0 1 0 1
    //         clock    1 1 0 0 0 1 1 1 0 0 0 1
    // Returns true if `clock` rose in this cycle, i.e. the SoC finished a cycle.
    auto single_cycle = [&]() -> bool {
        bool soc_rise = false;
        for (int level = 0; level <= 1; level++) {
            if (++half_cnt == clk_ratio) {
                half_cnt = 0;
                soc_clk ^= 1;
                if (soc_clk) soc_rise = true;
            }
            top->cpuClock = level;
            top->clock    = soc_clk;
            top->eval(); DUMP();
        }
        return soc_rise;
    };

    printf("--- Booting ysyxSoC (16550 UART + SPI flash), cpuClock = %u x clock ---\n", clk_ratio);

    // A UART receive line idles HIGH. Left at 0 (Verilator's default) the
    // UART16550 sees a permanent break condition.
    top->externalPins_uart0_rx = 1;
    // coreSel is declared on SimTop but not wired to asicTop; 0 is fine.
    top->coreSel = 0;

    // Reset for >= 100 cycles of the SLOW clock (handout requirement),
    // i.e. 100*K cpuClock cycles.
    top->reset = 1;
    for (int soc_cycles = 0; soc_cycles < 150; ) if (single_cycle()) soc_cycles++;
    top->reset = 0;
    printf("Reset released, running...\n");

    // Start the TX measurement from whatever the line settled to out of reset.
    tx_prev     = top->externalPins_uart0_tx;
    tx_run      = 0;
    tx_trace_on = (getenv("UART_TX_TRACE") != NULL);

    // Cycle limit: 400M by default; override with MAX_CYCLES=<n>, 0 = no limit.
    uint64_t MAX_CYCLES = 400000000ull;
    if (const char *s = getenv("MAX_CYCLES")) MAX_CYCLES = strtoull(s, NULL, 0);

    uint64_t cycle = 0;                   // cpuClock cycles since reset (= mcycle)
    uint64_t last_cycle = 0, last_inst = 0;

    while (simulation_running && (MAX_CYCLES == 0 || cycle < MAX_CYCLES)) {
        bool soc_tick = single_cycle();   // one cpuClock cycle
        cycle++;

        // UART and NVBoard live on the slow clock: service them once per
        // `clock` cycle. Per cpuClock cycle, NVBoard's 208-cycle divisor
        // would be off by a factor of K.
        if (soc_tick) {
            // --- measure how long TX holds each level ---
            uint8_t tx = top->externalPins_uart0_tx;
            if (tx == tx_prev) {
                tx_run++;
            } else {
                if (tx_run > 0) {
                    if (tx_run < tx_min) tx_min = tx_run;
                    if (tx_trace_on && tx_trace_left > 0) {
                        tx_trace_left--;
                        fprintf(stderr, "[uart-tx] level %u held %llu cycles (shortest %llu)\n",
                                tx_prev,
                                (unsigned long long)tx_run,
                                (unsigned long long)tx_min);
                    }
                }
                tx_prev = tx;
                tx_run  = 1;
            }
            nvboard_update();
        }

        // Heartbeat, so a hang is visibly distinguishable from "just slow".
        if ((cycle % 10000000ull) == 0) {
            uint64_t d_cyc = cycle - last_cycle;
            uint64_t d_ins = inst_count - last_inst;
            fprintf(stderr,
                    "[sim] %3llu M cyc | window: %8llu inst  IPC %.5f  CPI %7.1f | total IPC %.5f\n",
                    (unsigned long long)(cycle / 1000000ull),
                    (unsigned long long)d_ins,
                    d_cyc ? (double)d_ins / (double)d_cyc : 0.0,
                    d_ins ? (double)d_cyc / (double)d_ins : 0.0,
                    cycle ? (double)inst_count / (double)cycle : 0.0);
            last_cycle = cycle;
            last_inst  = inst_count;
        }
    }

    if (simulation_running) {
        printf("\n[sim] stopped after %llu cycles (limit reached)\n",
               (unsigned long long)cycle);
        sim_exit_code = 2;
    }

    if (tx_min != UINT64_MAX) {
        printf("[uart-tx] measured bit period = %llu clock cycles"
               " -> NVBOARD_UART_DIVISOR should be this value\n",
               (unsigned long long)tx_min);
    } else {
        printf("[uart-tx] TX line never changed level - nothing was transmitted\n");
    }

    printf("\n=== IPC (cpuClock = %u x clock) ===\n", clk_ratio);
    printf("  cycles       : %llu  (cpuClock)\n", (unsigned long long)cycle);
    printf("  instructions : %llu\n", (unsigned long long)inst_count);
    if (inst_count > 0) {
        printf("  IPC          : %.5f\n", (double)inst_count / (double)cycle);
        printf("  CPI          : %.2f\n", (double)cycle / (double)inst_count);
    }

    nvboard_hold();

    top->final();
#ifdef VCD_TRACE
    tfp->close();
    delete tfp;
#endif
    nvboard_quit();
    delete top;
    free(flash_mem);
    return sim_exit_code;
}
