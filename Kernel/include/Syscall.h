/**
 * OS003 系统调用库
 * 2026/7/10 Liu Chunyi
 */

#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <klib.h>

//系统调用号
#define SYS_exit       1    //退出
#define SYS_sleep      2    //睡眠
#define SYS_yield      3    //让出CPU
#define SYS_putchar    4    //打印字符

#define syscall(nr,arg1) ({\
    uint64_t __ret; \
    __asm__ volatile(\
        "syscall\n"\
        :"=a"(__ret)\
        :"a"(nr),"D"(arg1)\
        :"rcx","r11","rsi","rdx","r8","r9","r10","memory"\
    );\
    __ret;\
})

//SYSCALL/SYSRET per-CPU 结构
typedef struct {
    uint64_t kernel_rsp;    //+0: 当前线程内核栈顶
    uint64_t user_rsp;      //+8: SYSCALL 暂存用户 RSP
    uint64_t user_rcx;      //+16: SYSCALL 暂存用户 RIP
    uint64_t user_r11;      //+24: SYSCALL 暂存用户 RFLAGS
} syscall_cpu_t;

extern syscall_cpu_t syscall_cpu;
void init_syscall_msrs(void);//MSR初始化

#endif