//虚拟内存管理-分页管理
#ifndef _MEMORY_PAGING_H_
#define _MEMORY_PAGING_H_ 

#include <klib.h>
#include <mm/pmm.h>

// 页表索引位数（4KB 页，512 项/级）
#define PAGE_SHIFT       12
#define PAGE_SIZE        (1ULL << PAGE_SHIFT)
#define PAGE_MASK        (~(PAGE_SIZE - 1))

// 页表项标志位（参考 Intel/AMD 手册）
#define PTE_PRESENT         (1 << 0)   // 页存在
#define PTE_WRITABLE        (1 << 1)   // 可写
#define PTE_USER            (1 << 2)   // 用户模式可访问（用于用户态，内核通常不设）
#define PTE_WRITE_THROUGH   (1 << 3)   // 写透缓存
#define PTE_CACHE_DISABLE   (1 << 4)   // 禁用缓存
#define PTE_ACCESSED        (1 << 5)   // 已访问（CPU 设置）
#define PTE_DIRTY           (1 << 6)   // 已写入（仅用于末级页表）
#define PTE_HUGE            (1 << 7)   // 页面大小（0=4KB，1=2MB/1GB）
#define PTE_GLOBAL          (1 << 8)   // 全局页（TLB 不刷新）
#define PTE_NO_EXECUTE      (1ULL << 63)// NX 位（如果 CPU 支持）

// 虚拟地址分段提取索引的宏
#define PML4_INDEX(vaddr) (((vaddr) >> 39) & 0x1FF)
#define PDPT_INDEX(vaddr) (((vaddr) >> 30) & 0x1FF)
#define PD_INDEX(vaddr)   (((vaddr) >> 21) & 0x1FF)
#define PT_INDEX(vaddr)   (((vaddr) >> 12) & 0x1FF)

// 页表类型
typedef uint64_t pte_t;
typedef pte_t pml4_t[512];   // PML4 是 512 项

extern pml4_t* UEFI_PML4;
extern pml4_t* KernelPML4;

// 函数声明
void init_paging(void);//分页初始化
/**
 * @brief 映射一个虚拟地址到指定的物理地址
 * @param virt 虚拟地址
 * @param phys 物理地址
 * @param flags 页表项标志位
 */
void map_page(void *virt, void *phys, uint64_t flags);
void unmap_page(void *virt);//删除映射
void copy_pml4_entries(pml4_t *dest, int num_entries);
void copy_kernel_pml4_entries(pml4_t *dest);
void make_phys_writable(void *phys);
void replace_identity_pdpt(pml4_t *pml4);
/**
 * @brief 在指定页表中查询虚拟地址所对应的物理地址
 * @param pml4 页表指针
 * @param virt 虚拟地址
 * @return 物理地址
 */
uintptr_t virt_to_phys_on(pml4_t *pml4, void *virt);
uintptr_t virt_to_phys(void *virt);//在当前页表中查询虚拟地址所对应的物理地址

#endif