#include "clock.h"
#include "printk.h"
#include "types.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"
#include "buddy.h"
#include "vm.h"
#include "string.h"

extern struct task_struct* current;

void do_page_fault(struct pt_regs *regs) {
    printk("[S] Supervisor Page Fault, scause: %lx, stval: %lx\n", regs->scause, regs->stval);
    struct vm_area_struct* vma = find_vma(current, regs->stval);

    if (!vma) {
        printk("Invalid address.");
        return;
    }

    uint64_t page = alloc_page();
    create_mapping((uint64*)PA2VA((uint64_t)current->pgd), PGROUNDDOWN(regs->stval), VA2PA(page), 1, (vma->vm_flags & (~(uint64_t)VM_ANONYM)) | PTE_U_MASK | PTE_V_MASK);

    if (!(vma->vm_flags & VM_ANONYM)) { 
        uint64_t load_addr = (vma->file_offset_on_disk + vma->vm_content_offset_in_file);
        if (regs->stval - vma->vm_start < PGSIZE) {
            memcpy((void*)(page + PGOFFSET(regs->stval)), (void*)load_addr, MIN(PGSIZE - PGOFFSET(regs->stval), vma->vm_end - vma->vm_start));
        }
        else 
        {
            memcpy((void*)(page), (void*)(load_addr) + regs->stval - vma->vm_start, MIN(PGSIZE, vma->vm_end - regs->stval));
        }
    }

    vma->if_alloc = 1;
}


void trap_handler(uint64 scause, uint64 sepc, struct pt_regs *regs) {
    char catch = 0;
    uint64 interrupt_sig = 0x8000000000000000;
    if (scause & interrupt_sig) { 
        scause = scause - interrupt_sig;
        switch (scause) {
        case TIMER_INT: // it's Supervisor timer interrupt
            //printk("[S] Supervisor Mode Timer Interrupt\n");
            clock_set_next_event();
            do_timer();
            catch = 1;
            break;
        default:
            break;
        }
    }
    else { // it's exception
        switch (scause) {
        case ECALL_FROM_UMODE: 
            switch (regs->reg[16]) { 
            case SYS_WRITE:
                regs->reg[9] = sys_write(regs->reg[9], (const char*)regs->reg[10], regs->reg[11]);
                break;
            case SYS_GETPID:
                regs->reg[9] = sys_getpid();
                break;
            case SYS_CLONE:
                regs->reg[9] = sys_clone(regs);
                break;
            }
            regs->sepc += 4; // return to the next inst after ecall
            catch = 1;
            break;
        case INSTRUCTION_PAGE_FAULT: // demand paging
        case LOAD_PAGE_FAULT:
        case STORE_AMO_PAGE_FAULT:
            do_page_fault(regs);
            catch = 1;
            break;
        }
    }

    if (!catch) {
        printk("Unhandled trap: %llx\n", scause);
        printk("%lx\n", regs->stval);
        printk("%lx\n", sepc);
        while (1);
    }
}
