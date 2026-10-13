#include <am.h>
#include <stdint.h>

#define CYCLES_PER_US 1
#define CPU_FREQ_KHZ 100000

static uint64_t boot_cycles = 0;

static inline uint64_t read_mcycle(void) {
	uint32_t hi, lo, hi2;

	do {
		asm volatile ("csrr %0, mcycleh" : "=r"(hi));
		asm volatile ("csrr %0, mcycle" : "=r"(lo));
		asm volatile ("csrr %0, mcycleh" : "=r"(hi2));
	} while (hi != hi2);
	return ((uint64_t)hi << 32) | lo;
}

void __am_timer_init() {
	boot_cycles = read_mcycle();
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
	uptime->us = (read_mcycle() - boot_cycles) * 1000 / CPU_FREQ_KHZ;
}


void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}
