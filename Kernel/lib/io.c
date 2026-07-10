#include <io.h>

void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %b0, %w1" : : "a"(value), "Nd"(port) : "memory");
}
uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
//写16位到I/O端口
void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
// 往端口写入一个双字
void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %1, %0" : : "Nd"(port), "a"(val));
}

// IO等待延迟
void io_wait(void) {
    outb(0x80, 0);  // 往诊断端口写入任意值以产生IO延迟
}
uint64_t get_rip(void) {
    uint64_t rip;
    //call 指令会将返回地址（下一条指令）压栈
    asm volatile (
        "leaq (%%rip), %0\n\t" 
        "call 1f\n\t"           // 1: 是局部标签，call 会跳转到它
        "1:\n\t"                // 这是 call 要跳转到的位置
        "pop %0"                // 将返回地址（即此处的地址）弹出到变量
        : "=r" (rip)
        :
        : "memory"
    );
    return rip;
}
uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__ (
        "rdtsc"
        : "=a"(lo), "=d"(hi)
    );
    return ((uint64_t)hi << 32) | lo;
}

// 启用中断
void enable_interrupts(void) {
    asm volatile ("sti");
}

// 禁用中断
void disable_interrupts(void) {
    asm volatile ("cli");
}

//获取当前CR3寄存器的值
uint64_t get_cr3(void) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

//设置 CR3（切换页表）
void set_cr3(uint64_t cr3) {
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

uint64_t read_msr(uint32_t msr) {
    uint32_t eax, edx;
    __asm__ volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(msr));
    return ((uint64_t)edx << 32) | eax;
}