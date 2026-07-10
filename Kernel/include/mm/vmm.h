//VMM-虚拟内存管理器
#ifndef _MEMORY_VIRTUAL_MM_H_
#define _MEMORY_VIRTUAL_MM_H_ 

#include <klib.h>
#include <Memory.h>
#include <mm/pmm.h>
#include <mm/paging.h>

//内核虚拟地址
#define KernelVirtualMemoryStart 0xFFFF800000000000ULL
#define KernelVirtualMemoryEnd   0xFFFFFFFFFFFF0000ULL

//用户虚拟地址（起点设为 4GB，避开内核 identity mapping 可能占用的低 4GB）
#define UserVirtualMemoryStart   0x0000000100000000ULL
#define UserVirtualMemoryEnd     0x00007FFFFFFFFFFFULL

//虚拟内存描述符
typedef struct VMM_DESCRIPTOR {
    uintptr_t Address;          //起始地址
    uint64_t PageSize;          //页大小
    uint64_t Bytes;             //字节数
    uint8_t Reserved[8];        //保留
}__attribute__((packed)) VMM_DESCRIPTOR;

//分配记录结构体
typedef struct VMM_ALLOC_RECORD {
    uintptr_t Address;
    uint64_t Size;
} VMM_ALLOC_RECORD;

//虚拟内存管理结构体
typedef struct VMM_T {
    uint64_t flag;
    VMM_DESCRIPTOR* Descriptor;
    pml4_t*     Pml4;
    VMM_ALLOC_RECORD* AllocRecord;
    uint32_t AllocRecordNum;
    _Bool isUser;
} VMM_T;

extern VMM_T* Current_VMM_Desc;
extern int VMM_ERRORCODE;

/**
 * 初始化虚拟内存管理
 * @param flag 初始化标志
 * @return 虚拟内存管理结构体指针
 */
VMM_T* Init_Virtual_Memory_Manager(uint64_t flag);
VMM_T* Init_User_Virtual_Memory_Manager(uint64_t flag);
/**
 * @brief 切换当前虚拟内存管理器
 * @param vmm 虚拟内存管理结构体指针
 * @return 旧的虚拟内存管理结构体指针，如果没有则返回pml4指针
 */
uintptr_t Switch_Virtual_Mamanager(VMM_T* vmm);
uintptr_t Get_Current_VMM_CR3(void);//获取当前活动的VMM的页表基址
void* Vmm_Malloc(uint64_t size);
void Vmm_Free(void* virt, uint64_t size);

#endif