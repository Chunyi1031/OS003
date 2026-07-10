#include <int/gdt.h>

// GDT 在 x86_64 下每条条目 8 字节（系统描述符如 TSS 占 2 条）
#define GDT_BYTE_SIZE 64  // 8 条 * 8 字节
static uint8_t gdt_bytes[GDT_BYTE_SIZE] __attribute__((aligned(64)));

static uint8_t tss_storage[104] __attribute__((aligned(64)));

uint16_t KERNEL_CS = 0x08;  // index 1, offset 8
uint16_t KERNEL_DS = 0x10;  // index 2, offset 16
uint16_t USER_CS   = 0x1B;  // index 3, offset 24, RPL=3
uint16_t USER_DS   = 0x23;  // index 4, offset 32, RPL=3
// TSS: index 5+6, offset 40+48 => selector 0x28 + 0x30

void gdt_init(void) {
    // 清零 GDT
    __asm__ volatile(
        "cld\nrep stosb\n"
        :
        : "D"((uint64_t)gdt_bytes), "a"(0), "c"(GDT_BYTE_SIZE)
        : "memory"
    );

    // 清零 TSS
    __asm__ volatile(
        "cld\nrep stosb\n"
        :
        : "D"((uint64_t)tss_storage), "a"(0), "c"(sizeof(tss_storage))
        : "memory"
    );

    uint64_t gdt_base = (uint64_t)gdt_bytes;
    uint64_t tss_base = (uint64_t)tss_storage;

    // 所有 GDT 条目在 8 字节对齐的偏移处
    // 索引 1 (offset 8): Ring 0 代码段
    *(uint64_t*)(gdt_bytes + 8) = 0x00209A0000000000ULL;

    // 索引 2 (offset 16): Ring 0 数据段
    *(uint64_t*)(gdt_bytes + 16) = 0x0000920000000000ULL;

    // 索引 3 (offset 24): Ring 3 代码段
    *(uint64_t*)(gdt_bytes + 24) = 0x0020FA0000000000ULL;

    // 索引 4 (offset 32): Ring 3 数据段
    *(uint64_t*)(gdt_bytes + 32) = 0x0000F20000000000ULL;

    // 索引 5+6 (offset 40+48): TSS — 占 2 条（16 字节）
    uint8_t* tss_desc = gdt_bytes + 40;  // 选择子 0x28 指向这里
    tss_desc[0]  = 103;                     // limit[7:0]
    tss_desc[1]  = 0;                       // limit[15:8]
    tss_desc[2]  = tss_base & 0xFF;         // base[7:0]
    tss_desc[3]  = (tss_base >> 8) & 0xFF;  // base[15:8]
    tss_desc[4]  = (tss_base >> 16) & 0xFF; // base[23:16]
    tss_desc[5]  = 0x89;                    // access
    tss_desc[6]  = 0x00;                    // granularity
    tss_desc[7]  = (tss_base >> 24) & 0xFF; // base[31:24]
    tss_desc[8]  = (tss_base >> 32) & 0xFF; // base[39:32]
    tss_desc[9]  = (tss_base >> 40) & 0xFF; // base[47:40]
    tss_desc[10] = (tss_base >> 48) & 0xFF; // base[55:48]
    tss_desc[11] = (tss_base >> 56) & 0xFF; // base[63:56]
    tss_desc[12] = 0;
    tss_desc[13] = 0;
    tss_desc[14] = 0;
    tss_desc[15] = 0;

    // LGDT — 用 sub/add 安全分配栈空间
    uint16_t gdt_limit = GDT_BYTE_SIZE - 1;
    __asm__ volatile(
        "subq $16, %%rsp\n"
        "movw %0, (%%rsp)\n"
        "movq %1, 2(%%rsp)\n"
        "lgdt (%%rsp)\n"
        "addq $16, %%rsp\n"
        :
        : "r"(gdt_limit), "r"(gdt_base)
        : "memory"
    );

    // LTR — 选择子 0x28 = index 5, offset 40
    __asm__ volatile("ltr %w0" : : "a"(0x28));

    // 远跳转刷新 CS/DS/ES/SS
    __asm__ volatile(
        "pushq %0\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "retfq\n"
        "1:\n"
        "movq %1, %%rax\n"
        "movq %%rax, %%ds\n"
        "movq %%rax, %%es\n"
        "movq %%rax, %%ss\n"
        :
        : "r"((uint64_t)KERNEL_CS), "r"((uint64_t)KERNEL_DS)
        : "rax"
    );
}

void tss_set_rsp0(uint64_t rsp0) {
    // TSS 的 RSP0 字段位于 TSS 结构体偏移 4 字节处
    // tss_storage[4..11] = RSP0 (64-bit)
    *(uint64_t *)(tss_storage + 4) = rsp0;
}
