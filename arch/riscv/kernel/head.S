
.extern setup_vm
.extern early_pgtbl
.extern setup_vm_final
.extern mm_init
.extern task_init
.extern start_kernel

    .section .text.init
    .globl _start

_start:
    la sp, boot_stack_top
    call setup_vm
    call relocate
    call mm_init
    call setup_vm_final

    # set stvec = _traps
    la t0, _traps
    csrw stvec, t0
    
    # set sie[STIE] = 1
    csrr t0, sie
    ori t0, t0, 0x20
    csrw sie, t0
    
    # set first time interrupt
    jal ra, clock_set_next_event
    
    # set sstatus[SIE] = 1
    # csrr t0, sstatus
    # ori t0, t0, 0x2
    # csrw sstatus, t0

    call task_init
    call start_kernel

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)
    li t0, 0xffffffdf80000000
    add ra, ra, t0
    add sp, sp, t0

    # set satp with early_pgtbl
    la t0, early_pgtbl
    srli t0, t0, 12
    li t1, 0x8000000000000000
    or t0, t0, t1
    csrw satp, t0

    # flush tlb
    sfence.vma zero, zero
    ret
    
    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 
    .globl boot_stack_top
boot_stack_top: