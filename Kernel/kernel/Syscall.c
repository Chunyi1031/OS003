#include <Syscall.h>
#include <Memory.h>
#include <Thread.h>
#include <Print.h>
#include <Keyboard.h>

syscall_cpu_t syscall_cpu __attribute__((aligned(64)));

/*DeepSeek-V4-Flash*/
//SYSCALL 入口
__attribute__((naked))
void syscall_entry(void) {
    __asm__ volatile(
        "swapgs\n"
        "movq  %rsp, %gs:8\n"         // 保存用户 RSP
        "movq  %gs:0, %rsp\n"         // 加载内核栈顶
        "movq  %rcx, %gs:16\n"        // 保存用户 RIP
        "movq  %r11, %gs:24\n"        // 保存用户 RFLAGS

        "pushq %r15\n"
        "pushq %r14\n"
        "pushq %r13\n"
        "pushq %r12\n"
        "pushq %rbp\n"
        "pushq %rbx\n"

        // 参数布局: dispatch(nr, a1, a2, a3, a4, a5, a6)
        // 用户 regs → C ABI:
        // rax=nr  rdi=a1  rsi=a2  rdx=a3  r10=a4  r8=a5  r9=a6
        // C:      rdi=nr  rsi=a1  rdx=a2  rcx=a3  r8=a4  r9=a5  [rsp]=a6
        "pushq %r9\n"                  // arg6 入栈
        "subq  $8, %rsp\n"             // 16 字节对齐

        "movq  %r8, %r9\n"
        "movq  %r10, %r8\n"
        "movq  %rdx, %rcx\n"
        "movq  %rsi, %rdx\n"
        "movq  %rdi, %rsi\n"
        "movq  %rax, %rdi\n"

        "call  syscall_dispatch\n"

        "addq  $8, %rsp\n"             // 丢弃对齐
        "addq  $8, %rsp\n"             // 丢弃 arg6

        "popq  %rbx\n"
        "popq  %rbp\n"
        "popq  %r12\n"
        "popq  %r13\n"
        "popq  %r14\n"
        "popq  %r15\n"

        "movq  %gs:24, %r11\n"        // 恢复用户 RFLAGS
        "movq  %gs:16, %rcx\n"        // 恢复用户 RIP
        "movq  %gs:8, %rsp\n"         // 恢复用户 RSP

        "swapgs\n"
        "sysretq\n"
    );
}

//wrmsr辅助函数
static void wrmsr(uint32_t msr, uint64_t value) {
    __asm__ volatile("wrmsr"
        : : "a"((uint32_t)value), "d"((uint32_t)(value >> 32)), "c"(msr));
}

//MSR初始化
void init_syscall_msrs(void) {
    // ── 确保 CPU 支持并启用 SYSCALL ──
    // 读 IA32_EFER，置 SCE 位（bit 0）
    uint32_t efer_lo, efer_hi;
    __asm__ volatile("rdmsr" : "=a"(efer_lo), "=d"(efer_hi) : "c"(0xC0000080));
    efer_lo |= 1;                    // 设置 SCE (Syscall Enable)
    __asm__ volatile("wrmsr" : : "a"(efer_lo), "d"(efer_hi), "c"(0xC0000080));
    syscall_cpu.kernel_rsp = 0;     // 原内容不变
    syscall_cpu.kernel_rsp = 0;  // schedule() 负责设置
    syscall_cpu.user_rsp = 0;
    syscall_cpu.user_rcx = 0;
    syscall_cpu.user_r11 = 0;

    // IA32_STAR: SYSCALL CS=0x08  SYSRET CS=0x18(→0x1B)
    wrmsr(0xC0000081, ((uint64_t)0x18 << 48) | ((uint64_t)0x08 << 32));
    // IA32_LSTAR: syscall_entry 地址
    wrmsr(0xC0000082, (uint64_t)syscall_entry);
    // IA32_CSTAR: compat 不启用
    wrmsr(0xC0000083, 0);
    // IA32_FMASK: 进 syscall 时清除 IF
    wrmsr(0xC0000084, 0x200);
    // IA32_KERNEL_GS_BASE: per-CPU 结构体地址
    wrmsr(0xC0000102, (uint64_t)&syscall_cpu);
}
/*DeepSeek-V4-Flash-END*/

// void copy_from_user(void *kern_dst, void *user_src, uint64_t size) {
//     if (!SYSTEM_KernelThread->vmm) return;
//     //切换内核页表
//     uintptr_t old_cr3 = get_cr3();
//     set_cr3((uintptr_t)SYSTEM_KernelThread->vmm->Pml4);
//     if(size < PAGE_SIZE){
//         memcpy(kern_dst, (void*)virt_to_phys_on((pml4_t*)old_cr3,user_src), size);
//     }else{
//         int pages = size / PAGE_SIZE + 1;
//         //逐页复制
//         for(int i = 0;i < pages;i++){
//             void *kern_dst_page = kern_dst + i * PAGE_SIZE;
//             void *user_src_page = user_src + i * PAGE_SIZE;
//             memcpy(kern_dst_page, (void*)virt_to_phys_on((pml4_t*)old_cr3,user_src_page), PAGE_SIZE);
//         }
//     }
//     set_cr3(old_cr3);//恢复页表
// }

__attribute__((__externally_visible__))
void sys_sleep(uint64_t ms){
    ThreadSleepMS(ms);
}

//调度器
uint64_t syscall_dispatch(uint64_t nr, uint64_t arg1, uint64_t arg2,
                          uint64_t arg3, uint64_t arg4, uint64_t arg5,
                          uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    switch (nr) {
        case SYS_exit:
            thread_exit();
            return 0;
        case SYS_yield:
            schedule();
            return 0;
        case SYS_putchar:
            printf("%c", (int)(arg1 & 0xFF));
            return 0;
        case SYS_sleep:
            sys_sleep(arg1);
            return 0;
        default:
            return 0xFFFFFFFFFFFFFFFFULL;
    }
}
