#ifndef _IO_H_
#define _IO_H_ 

#include <types.h>

void outb(uint16_t port, uint8_t value);//向端口输出1字节
void outw(uint16_t port, uint16_t val);//向端口输出2字节
void outl(uint16_t port, uint32_t val);//向端口输出4字节
uint8_t inb(uint16_t port);//从端口读1字节
uint16_t inw(uint16_t port);//从端口读2字节
uint32_t inl(uint16_t port);//从端口读4字节
uint64_t rdtsc(void);//读取开机以来CPU振荡的次数
uint64_t get_rip(void);//获取RIP指针
void io_wait(void);//IO等待
void enable_interrupts(void);//开启中断
void disable_interrupts(void);//关闭中断
uint64_t get_cr3(void);//获取CR3
void set_cr3(uint64_t cr3);//设置CR3
void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);//CPUID
uint64_t read_msr(uint32_t msr);//读取MSR寄存器

#endif