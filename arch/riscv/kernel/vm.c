// arch/riscv/kernel/vm.c
#include "defs.h"
#include "types.h"
#include "vm.h"
#include "buddy.h"
#include "string.h"
#include "printk.h"
#include "proc.h"

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long early_pgtbl[512] __attribute__((__aligned__(0x1000)));

void setup_vm(void) {
    early_pgtbl[2] = (uint64)(0 | 0x20000000U | 15U);
    early_pgtbl[384] = (uint64)(0 | 0x20000000U | 15U);
    printk("...setup_vm done!\n");
}

unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

extern uint64 _stext;
extern uint64 _srodata;
extern uint64 _sdata;

void setup_vm_final(void) {
    memset(swapper_pg_dir, 0x0, PGSIZE);

    // mapping kernel text X|-|R|V
    create_mapping((uint64*)swapper_pg_dir, (uint64)&_stext, (uint64)(&_stext) - PA2VA_OFFSET,
        ((uint64)(&_srodata) - (uint64)(&_stext)) / PGSIZE, PTE_X_MASK | PTE_R_MASK | PTE_V_MASK);

    // mapping kernel rodata -|-|R|V
    create_mapping((uint64*)swapper_pg_dir, (uint64)&_srodata, (uint64)(&_srodata) - PA2VA_OFFSET,
        ((uint64)(&_sdata) - (uint64)(&_srodata)) / PGSIZE, PTE_R_MASK | PTE_V_MASK);

    // mapping other memory -|W|R|V
    create_mapping((uint64*)swapper_pg_dir, (uint64)&_sdata, (uint64)(&_sdata) - PA2VA_OFFSET,
        (PHY_END + PA2VA_OFFSET - (uint64)(&_sdata)) / PGSIZE, PTE_W_MASK | PTE_R_MASK | PTE_V_MASK);
    

    uint64 new_satp = (((uint64)swapper_pg_dir - PA2VA_OFFSET) >> 12);
    new_satp |= 0x8000000000000000;
    // set satp with swapper_pg_dir
    __asm__ volatile("csrw satp, %[base]":: [base] "r" (new_satp):);

    // flush TLB
    asm volatile("sfence.vma zero, zero");
    asm volatile("fence.i");
    printk("...setup_vm_final done!\n");
    return;
}

/* 创建多级页表映射关系 */
/*
    pgtbl 为根页表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小 (单位为4KB)
    perm 为映射的读写权限
*/
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    /*
    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */
    while (sz--) {
        uint64 vpn2 = ((va & 0x7fc0000000) >> 30);
        uint64 vpn1 = ((va & 0x3fe00000) >> 21);
        uint64 vpn0 = ((va & 0x1ff000) >> 12);

        // the second level page (next to root)
        uint64 *pgtbl1;
        if (!(pgtbl[vpn2] & 1)) {
            pgtbl1 = (uint64*)kalloc();
            pgtbl[vpn2] |= (1 | (((uint64)pgtbl1 - PA2VA_OFFSET) >> 2));
        }
        else pgtbl1 = (uint64*)(PA2VA_OFFSET + ((pgtbl[vpn2] & 0x3ffffffffffffc00) << 2));

        // the third level page
        uint64 *pgtbl0;
        if (!(pgtbl1[vpn1] & 1)) {
            pgtbl0 = (uint64*)kalloc();
            pgtbl1[vpn1] |= (1 | (((uint64)pgtbl0 - PA2VA_OFFSET) >> 2));
        }
        else pgtbl0 = (uint64*)(PA2VA_OFFSET + ((pgtbl1[vpn1] & 0x3ffffffffffffc00) << 2));

        pgtbl0[vpn0] = (perm | (pa >> 2));

        va += 0x1000, pa += 0x1000;
    }
}

int has_mapping(pagetable_t pgd, uint64_t addr) {
    uint64_t vpn0 = ((addr & 0x1ff000) >> 12);
    uint64_t vpn1 = ((addr & 0x3fe00000) >> 21);
    uint64_t vpn2 = ((addr & 0x7fc0000000) >> 30);

    if (!(pgd[vpn2] & 1)) return 0;
    pagetable_t pgtbl1 = (pagetable_t)(PA2VA_OFFSET + ((pgd[vpn2] & 0x3ffffffffffffc00) << 2));

    if (!(pgtbl1[vpn1] & 1)) return 0;
    pagetable_t pgtbl0 = (pagetable_t)(PA2VA_OFFSET + ((pgtbl1[vpn1] & 0x3ffffffffffffc00) << 2));
    
    if (pgtbl0[vpn0] & 1) return 1;
    else return 0;
}
