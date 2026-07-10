#ifndef _INT_GDT_H_
#define _INT_GDT_H_

/*deepseek-v4-flash*/
#include <klib.h>

#define GDT_ENTRIES 7

// GDT 描述符结构
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

// TSS 结构（x86_64，104 字节）
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;    // Ring 0 栈指针（中断/异常从用户态陷入时用）
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

// 外部全局选择子
extern uint16_t KERNEL_CS;   // Ring 0 代码段
extern uint16_t KERNEL_DS;   // Ring 0 数据段
extern uint16_t USER_CS;     // Ring 3 代码段
extern uint16_t USER_DS;     // Ring 3 数据段

void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);
/*deepseek-v4-flash-END*/

#endif