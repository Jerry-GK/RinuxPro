#include "sbi.h"
#include "clock.h"

unsigned long TIMECLOCK = 10000000;

unsigned long get_cycles() {
    unsigned long ret;
    __asm__ volatile (
        "rdtime %[ret]\n": [ret] "=r" (ret) ::
    );
    return ret;
}

void clock_set_next_event() {
    unsigned long next = get_cycles() + TIMECLOCK;
    sbi_ecall(0, 0, next, 0, 0, 0, 0, 0);
} 
