#include <mm/pmm.h>
#include <Print.h>

int32_t MemDescNum;
OS_MEMORY_DESCRIPTOR* MemDescAddr = NULL;

/*DeepSeek 漏洞修复*/
STATUS Init_Physical_Memory_Manager() {
    if (CheckMemoryMap(SYSTEM_MemoryMap) != 0) {
        out_error("Physical Memory Manager initialization failed: UEFI Memory Map not found\n");
        return 1;
    }
    uint32_t maxuednum = SYSTEM_MemoryMap->MapSize / SYSTEM_MemoryMap->DescriptorSize;
    MemDescAddr = (OS_MEMORY_DESCRIPTOR*)SYSTEM_MemoryMap->OsDescAddr;
    if (!MemDescAddr) {
        out_error("Physical Memory Manager initialization failed: UEFI Memory Desc not found\n");
        return 2;
    }

    MemDescNum = 0;  // 从 0 开始
    UEFI_MEMORY_DESCRIPTOR* edc;

    for (uint32_t a = 0; a < maxuednum; a++) {
        edc = AnalysisMemoryMap(a);
        if (!edc->PhysicalStart) continue;

        if (IsMemoryAvailable(edc)) {
            // 第一个可用描述符，或与上一个不连续，或类型变化 -> 新建
            if (MemDescNum == 0 ||
                MemDescAddr[MemDescNum - 1].Type != OS_AVAILABLE_MEMORY ||
                edc->PhysicalStart != (MemDescAddr[MemDescNum - 1].PhysicalStart +
                                       MemDescAddr[MemDescNum - 1].PageSize * PAGE_SIZE)) {
                // 初始化新描述符
                MemDescAddr[MemDescNum].PhysicalStart = edc->PhysicalStart;
                MemDescAddr[MemDescNum].Address = edc->VirtualStart;
                MemDescAddr[MemDescNum].PageSize = edc->NumberOfPages;
                MemDescAddr[MemDescNum].Type = OS_AVAILABLE_MEMORY;
                uint8_t *bitmap_bits = (uint8_t*)((uint64_t)&MemDescAddr[MemDescNum] + sizeof(OS_MEMORY_DESCRIPTOR));
                BitmapInit(&MemDescAddr[MemDescNum].bitmap, bitmap_bits, edc->NumberOfPages, 0);
                MemDescNum++;
            } else {
                // 合并到前一个描述符
                MemDescAddr[MemDescNum - 1].PageSize += edc->NumberOfPages;
                // 注意：合并时需要扩展位图并保留原分配信息，此处简化，实际应重设位图大小（不破坏已有标记）
                // 为简单演示，重新初始化位图（会丢失已分配标记，但避免越界更重要）
                uint8_t *new_bitmap_bits = (uint8_t*)((uint64_t)&MemDescAddr[MemDescNum - 1] + sizeof(OS_MEMORY_DESCRIPTOR));
                BitmapInit(&MemDescAddr[MemDescNum - 1].bitmap, new_bitmap_bits,
                           MemDescAddr[MemDescNum - 1].PageSize, 0);
            }
        } else {
            // 不可用内存处理，同样避免越界
            if (MemDescNum == 0 ||
                MemDescAddr[MemDescNum - 1].Type != OS_UNAVAILABLE_MEMORY ||
                edc->PhysicalStart != (MemDescAddr[MemDescNum - 1].PhysicalStart +
                                       MemDescAddr[MemDescNum - 1].PageSize * PAGE_SIZE)) {
                MemDescAddr[MemDescNum].PhysicalStart = edc->PhysicalStart;
                MemDescAddr[MemDescNum].Address = edc->VirtualStart;
                MemDescAddr[MemDescNum].PageSize = edc->NumberOfPages;
                MemDescAddr[MemDescNum].Type = OS_UNAVAILABLE_MEMORY;
                MemDescNum++;
            } else {
                MemDescAddr[MemDescNum - 1].PageSize += edc->NumberOfPages;
            }
        }
    }
    out_ok("Physical Memory Manager initialization successful\n", 0);
    return 0;
}

uint64_t GetMemoryTotalSize(){
    uint64_t size = 0;
    for(int a = 0;a < MemDescNum;a ++){
        size += MemDescAddr[a].PageSize*4;
    }
    return size;
}

void* Pmm_Malloc(int pages) {
    for (int i = 0; i < MemDescNum; i++) {  // 改为 <
        if (MemDescAddr[i].Type == OS_AVAILABLE_MEMORY) {
            int start_page_index = BitmapAllocBits(&MemDescAddr[i].bitmap, 0, pages);
            if (start_page_index != -1) {
                uint64_t physical_addr = MemDescAddr[i].PhysicalStart + (start_page_index << PAGE_SHIFT);
                return (void*)physical_addr;
            }
        }
    }
    return NULL;
}

void Pmm_Free(void* addr, int pages) {
    uint64_t target_addr = (uint64_t)addr;
    for (int i = 0; i < MemDescNum; i++) {  // 改为 <
        if (MemDescAddr[i].Type == OS_AVAILABLE_MEMORY) {
            uint64_t start_addr = MemDescAddr[i].PhysicalStart;
            uint64_t end_addr = MemDescAddr[i].PhysicalStart + (MemDescAddr[i].PageSize << PAGE_SHIFT);
            if (target_addr >= start_addr && target_addr < end_addr) {
                uint32_t page_index = (target_addr - start_addr) >> PAGE_SHIFT;
                BitmapSetBits(&MemDescAddr[i].bitmap, page_index, pages, 0);
                return;
            }
        }
    }
}