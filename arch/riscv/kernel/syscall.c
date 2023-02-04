#include "syscall.h"
#include "defs.h"
#include "printk.h"
#include "proc.h"
#include "string.h"
#include "vm.h"
#include "buddy.h"

extern struct task_struct* current;

long sys_write(unsigned int fd, const char* buf, int count) {
    uint64_t res = 0;
    for (int i = 0; i < count; i++) {
        if (fd == 1) {
            printk("%c", buf[i]);
            res++;
        }
    }
    return res;
}

long sys_getpid() {
    return current->pid;
}

extern struct task_struct* task[];
extern uint64_t __ret_from_fork;
extern unsigned long* swapper_pg_dir;

long sys_clone(struct pt_regs *regs) {
    int pid = 1; // the new pid
    while(task[pid] && pid < NR_TASKS) pid++; // find the first empty task_struct
    if (pid == NR_TASKS) { 
        printk("[S] Exceeds maximum thread number.\n");
        return -1;
    }

    uint64_t task_addr = alloc_page();
    task[pid] = (struct task_struct*)task_addr;
    memcpy(task[pid], current, PGSIZE); // copy PCB 
    task[pid]->pid = pid;
    task[pid]->thread.ra = (uint64_t)(&__ret_from_fork);
    
    struct pt_regs *child_pt_regs = (struct pt_regs*)(task_addr + PGOFFSET((uint64_t)regs));
    task[pid]->thread.sp = (uint64_t)child_pt_regs;

    child_pt_regs->reg[9] = 0; // child process return 0
    child_pt_regs->reg[1] = task[pid]->thread.sp; 
    child_pt_regs->sepc = regs->sepc + 4;

    // config page table
    task[pid]->pgd = (pagetable_t)alloc_page();
    // copy the root page
    memcpy((void*)(task[pid]->pgd), (void*)((&swapper_pg_dir)), PGSIZE);
    task[pid]->pgd = (pagetable_t)VA2PA((uint64_t)task[pid]->pgd); // turn physical address
    for (int i = 0; i < current->vma_cnt; i++) {
        struct vm_area_struct *vma = &(current->vmas[i]);
        if (vma->if_alloc) {
            uint64_t copy_head = vma->vm_start;
            while (copy_head < vma->vm_end) {
                if (!has_mapping((pagetable_t)PA2VA((uint64_t)current->pgd), PGROUNDDOWN(copy_head))) {
                    copy_head += PGSIZE;
                    continue;
                }
                uint64_t page = alloc_page();
                create_mapping((uint64*)PA2VA((uint64_t)task[pid]->pgd), PGROUNDDOWN(copy_head), VA2PA(page), 1, (vma->vm_flags & (~(uint64_t)VM_ANONYM)) | PTE_U_MASK | PTE_V_MASK);
                memcpy((void*)page, (void*)PGROUNDDOWN(copy_head), PGSIZE); // using kernel virtual address to copy
                copy_head += PGSIZE;
            }
        }
    }

    printk("[S] Clone Over, task[%d] has joint the ready queue\n", pid);
    return pid;
}
