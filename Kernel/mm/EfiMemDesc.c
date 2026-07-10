#include <Memory.h>
#include <mm/EfiMemDesc.h>
#include <Print.h>

//检查内存映射
STATUS CheckMemoryMap(UEFI_MEMORY_MAP* MemoryMap){
    STATUS status = 0;
    if(!MemoryMap->Buffer)status ++;
    if(MemoryMap->MapSize == 0)status ++;
    if(MemoryMap->DescriptorSize == 0)status ++;
    if(MemoryMap->DescriptorVersion == 0)status ++;
    if(!MemoryMap->StackAddr)status ++;
    return status;
}

_Bool IsMemoryAvailable(UEFI_MEMORY_DESCRIPTOR *Desc) {
    UEFI_MEMORY_MAP* MemoryMap = SYSTEM_MemoryMap;
    if(Desc->PhysicalStart == (uintptr_t)MemoryMap->StackAddr){
        return false;
    }
    if((Desc->Type==CONVENTIONAL_MEMORY) || (Desc->Type==BOOT_SERVICES_CODE) || (Desc->Type==BOOT_SERVICES_DATA)){
        return true;
    }
    return false;
}

//解析并打印描述符信息
UEFI_MEMORY_DESCRIPTOR* AnalysisMemoryMap(const uint32_t num){
    UEFI_MEMORY_MAP* MemoryMap = SYSTEM_MemoryMap;
    UEFI_MEMORY_DESCRIPTOR* DescriptorAddr = (UEFI_MEMORY_DESCRIPTOR*)((uint8_t*)MemoryMap->Buffer + MemoryMap->DescriptorSize * num);//描述符的位置
    uint32_t maxDescriptors = MemoryMap->MapSize / MemoryMap->DescriptorSize;//计算描述符数量
    if (num >= maxDescriptors) {
        out_error("Index out of bounds\n");
        return NULL;
    }
    return DescriptorAddr;
}

//处理属性
void PrintMemoryAttributes(uint64_t Attribute) {
    printf("Attribute:    \t%X (", Attribute);
    int first = 1;
    int hasValidAttribute = 0;
    if (Attribute & MEMORY_UC) {
        printf("%sUncached", first ? "" : " | ");
        first = 0;
        hasValidAttribute = 1;
    }
    if (Attribute & MEMORY_WC) {
        printf("%sWrite Combining", first ? "" : " | ");
        first = 0;
        hasValidAttribute = 1;
    }
    if (Attribute & MEMORY_WT) {
        printf("%sWrite Through", first ? "" : " | ");
        first = 0;
        hasValidAttribute = 1;
    }
    if (Attribute & MEMORY_WB) {
        printf("%sWrite Back", first ? "" : " | ");
        first = 0;
        hasValidAttribute = 1;
    }
    if (Attribute & MEMORY_UCE) {
        printf("%sUncached Export", first ? "" : " | ");
        first = 0;
        hasValidAttribute = 1;
    }
    if (Attribute & MEMORY_WP) {
        printf("%sWrite Protect", first ? "" : " | ");
        first = 0;
        hasValidAttribute = 1;
    }
    if (Attribute & MEMORY_RP) {
        printf("%sRead Protect", first ? "" : " | ");
        first = 0;
        hasValidAttribute = 1;
    }
    if (Attribute & MEMORY_XP) {
        printf("%sExecute Protect", first ? "" : " | ");
        first = 0;
        hasValidAttribute = 1;
    }
    if (Attribute & MEMORY_RO) {
        printf("%sRead Only", first ? "" : " | ");
        first = 0;
        hasValidAttribute = 1;
    }

    // 如果没有任何有效属性，输出提示
    if (!hasValidAttribute) {
        printf("None/Unknown");
    }

    printf(")\n");
}
void PrintMemoryDescriptor(UEFI_MEMORY_DESCRIPTOR* DescriptorAddr){
    int sizekb = DescriptorAddr->NumberOfPages * 4096 / 1024;
    printf("Memory descriptor at %X\n",(uint64_t)DescriptorAddr);//打印地址
    printf("Type:         \t%d\n",DescriptorAddr->Type);//Type
    printf("PhysicalStart:\t%x\n",DescriptorAddr->PhysicalStart);//物理地址
    printf("VirtualStart: \t%x\n",DescriptorAddr->VirtualStart);//虚拟地址
    printf("Pages:        \t%d\n",DescriptorAddr->NumberOfPages);//页数
    if(sizekb < 1024){
        printf("Size:         \t%dKB\n",sizekb);
    }else{
        printf("Size:         \t%dMB\n",sizekb / 1024);
    }
    printf("Available:    \t%s\n",((IsMemoryAvailable(DescriptorAddr)) ? "True" : "False"));
    PrintMemoryAttributes(DescriptorAddr->Attribute);
}

//打印可用指定数量的内存的编号
void PrintAvailableMemoryDescriptor(const uint32_t num){
    UEFI_MEMORY_MAP* MemoryMap = SYSTEM_MemoryMap;
    uint32_t maxDescriptors = MemoryMap->MapSize / MemoryMap->DescriptorSize;//计算描述符数量
    if (num >= maxDescriptors) {
        out_error("Index out of bounds\n");
        return;
    }
    int TotalNum = 0;
    UEFI_MEMORY_DESCRIPTOR* DescriptorAddr = NULL;
    //遍历所有描述符
    for(int a = 0;a < maxDescriptors;a ++){
        DescriptorAddr = AnalysisMemoryMap(a);
        //检查是否可用
        if(IsMemoryAvailable(DescriptorAddr) != 0){
            printf("%d ",a);
            TotalNum ++;
        }
        if(TotalNum >= num)break;//数量足够则跳出循环
    }
    printf("\nTotal:%d\n",TotalNum);
}
UEFI_MEMORY_DESCRIPTOR* FindLargestAvailableBlock() {
    UEFI_MEMORY_MAP* MemoryMap = SYSTEM_MemoryMap;
    UEFI_MEMORY_DESCRIPTOR* largest = NULL;
    uint64_t largestSize = 0;
    uint32_t maxDescriptors = MemoryMap->MapSize / MemoryMap->DescriptorSize; // 计算最大描述符数量
    // 遍历每个描述符
    for (uint32_t i = 0; i < maxDescriptors; i++) {
        UEFI_MEMORY_DESCRIPTOR* Desc = (UEFI_MEMORY_DESCRIPTOR*)((uint8_t*)MemoryMap->Buffer + i * MemoryMap->DescriptorSize); // 计算描述符所在内存地址
        // 只考虑完全可用的常规内存
        if (Desc->Type == CONVENTIONAL_MEMORY) {
            uint64_t size = Desc->NumberOfPages * 4096;
            if (size > largestSize) {
                largestSize = size;
                largest = Desc;
            }
        }
    }
    return largest;
}
