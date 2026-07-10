#ifndef _INT_IDT_H_
#define _INT_IDT_H_

#include <klib.h>
#include "pic.h"

// 异常向量号宏定义（0-31）
#define IDT_DE_VECTOR   0   // 除法错误 (#DE)
#define IDT_DB_VECTOR   1   // 调试异常 (#DB)
#define IDT_NMI_VECTOR  2   // 非屏蔽中断 (NMI)
#define IDT_BP_VECTOR   3   // 断点 (#BP)
#define IDT_OF_VECTOR   4   // 溢出 (#OF)
#define IDT_BR_VECTOR   5   // 边界范围 (#BR)
#define IDT_UD_VECTOR   6   // 无效操作码 (#UD)
#define IDT_NM_VECTOR   7   // 设备不可用 (#NM)
#define IDT_DF_VECTOR   8   // 双重故障 (#DF)
// 向量 9 保留（协处理器段超限，386后不再使用）
#define IDT_TS_VECTOR   10  // 无效 TSS (#TS)
#define IDT_NP_VECTOR   11  // 段不存在 (#NP)
#define IDT_SS_VECTOR   12  // 堆栈段错误 (#SS)
#define IDT_GP_VECTOR   13  // 通用保护故障 (#GP)
#define IDT_PF_VECTOR   14  // 页故障 (#PF)
// 向量 15 保留
#define IDT_MF_VECTOR   16  // x87 浮点错误 (#MF)
#define IDT_AC_VECTOR   17  // 对齐检查 (#AC)
#define IDT_MC_VECTOR   18  // 机器检查 (#MC)
#define IDT_XM_VECTOR   19  // SIMD 浮点异常 (#XM)
#define IDT_VE_VECTOR   20  // 虚拟化异常 (#VE)
// 向量 21–31 保留

#define INTERRUPT_GATE_SIZE     256

#define X86_64_INTERRUPT_GATE   0x8E //x86_64中断门
#define X86_64_TRAP_GATE        0x8F //x86_64陷阱门
#define X86_64_INTERRUPT_GATE_USER 0xEE // x86_64中断门, DPL=3（Ring 3可触发）

//中断描述符表（IDT）结构体定义
typedef struct GATE_DESC {
    uint16_t offset_1;        //偏移0..15
    uint16_t selector;        //GDT和LDT选择子
    uint8_t  ist;             //位0至2为中断堆栈表偏移量，其余位为零。
    uint8_t  type_attributes; //门类型、DPL和P字段
    uint16_t offset_2;        //偏移16..31
    uint32_t offset_3;        //偏移32..63
    uint32_t reserved;        //保留，必须为零
} __attribute__((packed)) GATE_DESC;
//IDTR
typedef struct IDTR {
    uint16_t limit; //IDT表界限
    uint32_t base_l;  //IDT表基地址(低32位)
    uint32_t base_h;  //IDT表基地址(高32位)
} __attribute__((packed)) IDTR;

extern GATE_DESC idt_table[INTERRUPT_GATE_SIZE];//IDT表
extern uint16_t UEFI_CS, UEFI_DS;

STATUS Init_IDT();//初始化IDT
void Init_Interrupt();//初始化中断
void set_intr_gate(uint32_t n, const void *addr,uint8_t type);//设置中断门
void set_irq_gate(uint32_t n, const void *addr);//设置IRQ
void lidt(uint64_t addr,uint16_t size);//加载IDT

void interrupt_handler_default();//通用中断服务程序处理函数路口
void interrupt_handler_0(void);
void interrupt_handler_1(void);
void interrupt_handler_2(void);
void interrupt_handler_3(void);
void interrupt_handler_4(void);
void interrupt_handler_5(void);
void interrupt_handler_6(void);
void interrupt_handler_7(void);
void interrupt_handler_8(void);
void interrupt_handler_10(void);
void interrupt_handler_11(void);
void interrupt_handler_12(void);
void interrupt_handler_13(void);
void interrupt_handler_14(void);
void interrupt_handler_16(void);
void interrupt_handler_17(void);
void interrupt_handler_18(void);
void interrupt_handler_19(void);
void interrupt_handler_20(void);
void interrupt_handler_0x80(void);

void interrupt_default();//通用中断服务程序处理函数
void interrupt_DE();
void interrupt_DB();
void interrupt_NMI();
void interrupt_BP();
void interrupt_BR();
void interrupt_OF();
void interrupt_UD();
void interrupt_NM();
void interrupt_DF();
void interrupt_NP();
void interrupt_SS();
void interrupt_MF();
void interrupt_AC();
void interrupt_MC();
void interrupt_XM();
void interrupt_VE();
void interrupt_PF(uint64_t fault_addr, uint64_t error_code);
void interrupt_GP(uint64_t error_code, uint64_t rip);

#endif