#include "printk.h"
#include "sbi.h"
#include "defs.h"
#include "proc.h"

extern void test();

int start_kernel() {
    printk("[S] Welcome to use Rinux\n");
    schedule();
    test(); // DO NOT DELETE !!!

	return 0;
}
