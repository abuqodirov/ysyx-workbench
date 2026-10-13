#include <am.h>
#include <klib-macros.h>

extern char _heap_start;
int main(const char *args);

extern char _pmem_start;
#define PMEM_SIZE (128 * 1024 * 1024)
#define PMEM_END  ((uintptr_t)&_pmem_start + PMEM_SIZE)

Area heap = RANGE(&_heap_start, PMEM_END);
static const char mainargs[MAINARGS_MAX_LEN] = TOSTRING(MAINARGS_PLACEHOLDER); // defined in CFLAGS

#define UART_BASE 0x10000000ul
#define UART_THR ((volatile uint8_t *)(UART_BASE + 0))
#define UART_DLL ((volatile uint8_t *)(UART_BASE + 0))
#define UART_DLM ((volatile uint8_t *)(UART_BASE + 1))
#define UART_FCR ((volatile uint8_t *)(UART_BASE + 2))
#define UART_LCR ((volatile uint8_t *)(UART_BASE + 3))
#define UART_LSR ((volatile uint8_t *)(UART_BASE + 5))

#define LCR_8N1 0x03
#define LCR_DLAB 0x80
#define LSR_THRE 0x20
#define FCR_ENA 0x07

#define UART_CLK_HZ 25000000
#define UART_BAUD 115200
#define UART_DIVISOR ((UART_CLK_HZ) / (16 * (UART_BAUD)))   // 13

static void uart_init(void) {
	*UART_LCR = LCR_DLAB | LCR_8N1;
	*UART_DLL = (uint8_t)(UART_DIVISOR & 0xff);
	*UART_DLM = (uint8_t)((UART_DIVISOR >> 8) & 0xff);
	*UART_LCR = LCR_8N1;
	*UART_FCR = FCR_ENA;
}

void putch(char ch) {
	while ((*UART_LSR & LSR_THRE) == 0){
	
	}
	*UART_THR = (uint8_t)ch;
}

void halt(int code) {
  asm volatile ("mv a0, %0; ebreak" : :"r"(code));
  while (1);
}

void _trm_init() {
  uart_init();
	int ret = main(mainargs);
  halt(ret);
}
