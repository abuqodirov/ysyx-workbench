// VPI memory model for the Icarus Verilog build of NpcSim (E8 four-state sim).
// Icarus has no DPI-C, so this registers system tasks/functions instead:
//   $pmem_load                          load the image named by +IMG=<path> at 0x8000_0000
//   $pmem_read(addr)            -> word  read the aligned 32-bit word        (function)
//   $pmem_write(addr, data, mask)        byte-masked write                    (task)
// Same address map as core/standalone/sim_main.cpp: memory, UART16550, GPIO.
// Unlike Verilator, arguments can be X or Z here. X in an address or mask
// stops the simulation (the core has lost track of what it is doing); X in
// store DATA only warns, because saving a never-written register is legal.
#include <vpi_user.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PMEM_BASE 0x80000000u
#define PMEM_SIZE (128u * 1024 * 1024)
#define UART_BASE 0x10000000u      // UART16550: THR/DLL @+0, LCR @+3, LSR @+5
#define UART_SIZE 0x1000u
#define GPIO_BASE 0x20001000u      // LEDs / switches / 7-seg: writes ignored, reads 0
#define GPIO_SIZE 0x10u

static uint8_t *pmem           = NULL;
static uint8_t  uart_lcr       = 0;    // bit 7 (DLAB) set -> offset 0 is the divisor
static int      xdata_warnings = 0;

static unsigned long long now(void) {
    s_vpi_time t;
    t.type = vpiSimTime;
    vpi_get_time(NULL, &t);
    return ((unsigned long long)t.high << 32) | t.low;
}

static void stop(const char *why, uint32_t addr) {
    vpi_printf("\n[vpi] %s (0x%08x) at time %llu -- stopping\n", why, addr, now());
    vpi_control(vpiFinish, 1);
}

// Fetch the next argument of the current call as 32 bits.
// *xbits receives the X/Z mask (VPI's bval); X/Z bits read back as 0.
static uint32_t next_arg(vpiHandle it, uint32_t *xbits) {
    s_vpi_value v;
    v.format = vpiVectorVal;
    vpi_get_value(vpi_scan(it), &v);
    *xbits = (uint32_t)v.value.vector[0].bval;
    return (uint32_t)(v.value.vector[0].aval & ~v.value.vector[0].bval);
}

static uint32_t mem_read(uint32_t addr) {
    uint32_t a = addr & ~3u, v;
    if (a - PMEM_BASE < PMEM_SIZE) { memcpy(&v, pmem + (a - PMEM_BASE), 4); return v; }
		if (a == 0x30000000u) return 0x800002b7u; 
    if (a == 0x30000004u) return 0x00028067u;
    if (a == UART_BASE + 4) return 0x60u << 8;      // LSR (+5) in byte lane 1: transmitter empty
    if (a - UART_BASE < UART_SIZE || a - GPIO_BASE < GPIO_SIZE) return 0;
    stop("read of unmapped address", addr);
    return 0;
}

static void mem_write(uint32_t addr, uint32_t data, uint32_t mask) {
    uint32_t a = addr & ~3u;
    if (a - PMEM_BASE < PMEM_SIZE) {
        for (int i = 0; i < 4; i++)
            if (mask & (1u << i)) pmem[a - PMEM_BASE + i] = (uint8_t)(data >> (8 * i));
        return;
    }
    if (a == UART_BASE) {                           // the word holding THR/DLL (+0) and LCR (+3)
        if (mask & 0x8) uart_lcr = (uint8_t)(data >> 24);
        if ((mask & 0x1) && !(uart_lcr & 0x80)) { vpi_printf("%c", (int)(data & 0xff)); vpi_flush(); }
        return;
    }
    if (a - UART_BASE < UART_SIZE || a - GPIO_BASE < GPIO_SIZE) return;
    stop("write to unmapped address", addr);
}

static PLI_INT32 pmem_load_calltf(PLI_BYTE8 *ud) {
    (void)ud;
    s_vpi_vlog_info info;
    const char *path = NULL;
    vpi_get_vlog_info(&info);
    for (int i = 0; i < info.argc; i++)
        if (strncmp(info.argv[i], "+IMG=", 5) == 0) path = info.argv[i] + 5;

    if (!pmem) pmem = calloc(PMEM_SIZE, 1);
    FILE *fp = path ? fopen(path, "rb") : NULL;
    if (!pmem || !fp) {
        vpi_printf("[vpi] cannot load image '%s' (pass +IMG=<path>)\n", path ? path : "");
        vpi_control(vpiFinish, 1);
        return 0;
    }
    size_t n = fread(pmem, 1, PMEM_SIZE, fp);
    fclose(fp);
    vpi_printf("Loaded %s (%zu bytes) at 0x%08x\n", path, n, PMEM_BASE);
    return 0;
}

static PLI_INT32 pmem_read_calltf(PLI_BYTE8 *ud) {
    (void)ud;
    vpiHandle call = vpi_handle(vpiSysTfCall, NULL);
    vpiHandle it   = vpi_iterate(vpiArgument, call);
    uint32_t  x, addr = next_arg(it, &x);
    vpi_free_object(it);

    uint32_t data = 0;
    if (x) stop("$pmem_read: address has X/Z bits", addr);
    else   data = mem_read(addr);

    s_vpi_value v;                                  // the function's return value
    v.format        = vpiIntVal;
    v.value.integer = (PLI_INT32)data;
    vpi_put_value(call, &v, NULL, vpiNoDelay);
    return 0;
}

static PLI_INT32 pmem_read_sizetf(PLI_BYTE8 *ud) {
    (void)ud;
    return 32;                                      // $pmem_read returns 32 bits
}

static PLI_INT32 pmem_write_calltf(PLI_BYTE8 *ud) {
    (void)ud;
    vpiHandle it = vpi_iterate(vpiArgument, vpi_handle(vpiSysTfCall, NULL));
    uint32_t  xa, xd, xm;
    uint32_t  addr = next_arg(it, &xa);
    uint32_t  data = next_arg(it, &xd);
    uint32_t  mask = next_arg(it, &xm);
    vpi_free_object(it);

    if (xa || xm) { stop("$pmem_write: address or mask has X/Z bits", addr); return 0; }
    if (xd && xdata_warnings++ < 5)
        vpi_printf("[vpi] note: store of X data to 0x%08x at time %llu (X bits stored as 0)\n",
                   addr, now());
    mem_write(addr, data, mask);
    return 0;
}

static void register_pmem(void) {
    s_vpi_systf_data tf;

    memset(&tf, 0, sizeof tf);
    tf.type   = vpiSysTask;
    tf.tfname = "$pmem_load";
    tf.calltf = pmem_load_calltf;
    vpi_register_systf(&tf);

    memset(&tf, 0, sizeof tf);
    tf.type   = vpiSysTask;
    tf.tfname = "$pmem_write";
    tf.calltf = pmem_write_calltf;
    vpi_register_systf(&tf);

    memset(&tf, 0, sizeof tf);
    tf.type        = vpiSysFunc;
    tf.sysfunctype = vpiSizedFunc;
    tf.tfname      = "$pmem_read";
    tf.calltf      = pmem_read_calltf;
    tf.sizetf      = pmem_read_sizetf;
    vpi_register_systf(&tf);
}

// vvp calls everything listed here when it loads this module.
void (*vlog_startup_routines[])(void) = { register_pmem, 0 };

