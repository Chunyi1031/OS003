#include <mm/vmm.h>
#include <Print.h>

int VMM_ERRORCODE = 0;

uintptr_t Get_Current_VMM_CR3(void) {
    return Current_VMM_Desc ? (uintptr_t)Current_VMM_Desc->Pml4 : get_cr3();
}

/*deepseek*/
// 内部辅助函数：在指定页表中映射一段连续内存（虚拟连续，物理离散分配）
int vmm_map_range(uintptr_t virt_start, void* phys_pages[], uint64_t page_count, uint64_t flags) {
    for (uint64_t i = 0; i < page_count; i++) {
        void* phys = phys_pages[i];
        if (!phys) return -1;
        // 注意：map_page 默认使用当前 CR3 的页表，我们需要一个能指定页表的版本
        // 但原 map_page 使用 get_cr3()，无法指定。故在此临时切换 CR3，映射后恢复。
        map_page((void*)(virt_start + i * PAGE_SIZE), phys, flags);
    }
    return 0;
}
/*deepseek-END*/

VMM_T* Init_Virtual_Memory_Manager(uint64_t flag){
    //为VMM_T分配内存
    VMM_T* vmm = Pmm_Malloc(1);
    if(!vmm){
        VMM_ERRORCODE = 1;
        return NULL;
    }
    memset(vmm,0,PAGE_SIZE);
    vmm->flag = flag;
    //分配pml4并初始化
    pml4_t* new_pml4 = (pml4_t*)Pmm_Malloc(1);
    if(!new_pml4){
        Pmm_Free(vmm,1);
        VMM_ERRORCODE = 2;
        return NULL;
    }
    memset(new_pml4,0,PAGE_SIZE);
    copy_pml4_entries(new_pml4,512);
    replace_identity_pdpt(new_pml4);
    //为VMM_DESCRIPTOR分配内存
    void* desc_phys = Pmm_Malloc(1);
    if(!desc_phys){
        Pmm_Free(vmm,1);
        Pmm_Free(new_pml4,1);
        VMM_ERRORCODE = 3;
        return NULL;
    }
    memset(desc_phys, 0, PAGE_SIZE);
    uintptr_t saved_cr3 = get_cr3();
    set_cr3((uintptr_t)new_pml4);//切换新页表
    map_page((void*)KernelVirtualMemoryStart, desc_phys, flag);
    //更新vmm
    vmm->Descriptor = (VMM_DESCRIPTOR*)KernelVirtualMemoryStart;
    vmm->Descriptor->Address = KernelVirtualMemoryStart + PAGE_SIZE;
    vmm->Descriptor->Bytes = 0;
    vmm->Descriptor->PageSize = 0;
    vmm->flag = flag;
    vmm->Pml4 = new_pml4;
    vmm->AllocRecordNum = 0;
    set_cr3(saved_cr3);//恢复旧页表
    VMM_ERRORCODE = 0;
    return vmm;
}

VMM_T* Init_User_Virtual_Memory_Manager(uint64_t flag){
    //为VMM_T分配内存
    VMM_T* vmm = Pmm_Malloc(1);
    if(!vmm){
        VMM_ERRORCODE = 1;
        return NULL;
    }
    memset(vmm,0,PAGE_SIZE);
    vmm->isUser = true;
    vmm->flag = flag;
    //分配pml4并初始化
    pml4_t* new_pml4 = (pml4_t*)Pmm_Malloc(1);
    if(!new_pml4){
        Pmm_Free(vmm,1);
        VMM_ERRORCODE = 2;
        return NULL;
    }
    memset(new_pml4,0,PAGE_SIZE);
    copy_kernel_pml4_entries(new_pml4);
    replace_identity_pdpt(new_pml4);
    //为VMM_DESCRIPTOR分配内存
    void* desc_phys = Pmm_Malloc(1);
    if(!desc_phys){
        Pmm_Free(vmm,1);
        Pmm_Free(new_pml4,1);
        VMM_ERRORCODE = 3;
        return NULL;
    }
    memset(desc_phys, 0, PAGE_SIZE);
    uintptr_t saved_cr3 = get_cr3();
    set_cr3((uintptr_t)new_pml4);
    map_page((void*)UserVirtualMemoryStart, desc_phys, flag);
    //更新vmm
    vmm->Descriptor = (VMM_DESCRIPTOR*)UserVirtualMemoryStart;
    vmm->Descriptor->Address = UserVirtualMemoryStart + PAGE_SIZE;
    vmm->Descriptor->Bytes = 0;
    vmm->Descriptor->PageSize = 0;
    vmm->flag = flag;
    vmm->Pml4 = new_pml4;
    vmm->AllocRecordNum = 0;
    set_cr3(saved_cr3);
    VMM_ERRORCODE = 0;
    return vmm;
}

uintptr_t Switch_Virtual_Mamanager(VMM_T* vmm){
    if(!vmm){
        VMM_ERRORCODE = 10;
        return 0;
    }
    uintptr_t old_vmm;
    //如果当前有虚拟内存管理器
    if(Current_VMM_Desc){
        old_vmm = (uintptr_t)Current_VMM_Desc;
    //否则直接获取当前页表
    }else{
        old_vmm = get_cr3();
    }
    Current_VMM_Desc = vmm;
    set_cr3((uintptr_t)Current_VMM_Desc->Pml4);//切换新页表
    VMM_ERRORCODE = 0;
    return old_vmm;
}

void* Vmm_Malloc(uint64_t size){
    //检查数据有效性
    if(!size) return NULL;
    if(!Current_VMM_Desc->Descriptor){
        VMM_ERRORCODE = 1;
        return NULL;
    }
    //计算数据
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t alloc_bytes = pages * PAGE_SIZE;
    VMM_DESCRIPTOR* desc = Current_VMM_Desc->Descriptor;
    //检查内存是否足够（根据 isUser 选择对应的地址上限）
    uint64_t vmm_end = Current_VMM_Desc->isUser ? UserVirtualMemoryEnd : KernelVirtualMemoryEnd;
    if(desc->Address + alloc_bytes > vmm_end){
        VMM_ERRORCODE = 2;
        return NULL;
    }
    /*deepseek*/
    uintptr_t virt_start = desc->Address;
    // 分配物理页框
    void* phys_pages[pages];
    for (uint64_t i = 0; i < pages; i++) {
        phys_pages[i] = Pmm_Malloc(1);
        if (phys_pages[i] == 0) {
            // 分配失败，回滚已分配的物理页
            for (uint64_t j = 0; j < i; j++) {
                Pmm_Free(phys_pages[j], 1);
            }
            VMM_ERRORCODE = 3;
            return NULL;
        }
    }
    /*deepseek-END*/
    if(vmm_map_range(virt_start,phys_pages,pages,Current_VMM_Desc->flag) != 0){
        for (uint64_t i = 0; i < pages; i++) {
            Pmm_Free(phys_pages[i], 1);
        }
        return NULL;
    }
    //更新描述符
    desc->Address += alloc_bytes;
    desc->Bytes += alloc_bytes;
    desc->PageSize += pages;
    Current_VMM_Desc->AllocRecord[Current_VMM_Desc->AllocRecordNum].Address = virt_start;
    Current_VMM_Desc->AllocRecord[Current_VMM_Desc->AllocRecordNum].Size = alloc_bytes;
    Current_VMM_Desc->AllocRecordNum++;
    VMM_ERRORCODE = 0;
    return (void*)virt_start;
}

/*deepseek*/
void Vmm_Free(void* virt, uint64_t size) {
    if (!Current_VMM_Desc || !virt || size == 0) return;
    uintptr_t vaddr = (uintptr_t)virt;
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    VMM_DESCRIPTOR* desc = Current_VMM_Desc->Descriptor;

    // 简单检查：释放的地址必须在已分配区间内
    uint64_t virt_start = Current_VMM_Desc->isUser ? UserVirtualMemoryStart : KernelVirtualMemoryStart;
    if ((vaddr < (virt_start + PAGE_SIZE)) || (vaddr > desc->Address)) {
        VMM_ERRORCODE = 1;
        return;
    }

    // 逐个页面解除映射并释放物理内存
    for (uint64_t i = 0; i < pages; i++) {
        uintptr_t phys = virt_to_phys((void*)(vaddr + i * PAGE_SIZE));
        if (phys) {
            unmap_page((void*)(vaddr + i * PAGE_SIZE));
            Pmm_Free((void*)phys, 1);
        }
    }

    // 更新描述符（注意：不进行地址合并，只减少统计，实际基址不回退，避免碎片）
    desc->Bytes -= pages * PAGE_SIZE;
    desc->PageSize -= pages;
    Current_VMM_Desc->AllocRecordNum--;
    VMM_ERRORCODE = 0;
}
/*deepseek-END*/