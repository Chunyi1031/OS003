#include <ACPI.h>

RSDP_DESCRIPTOR *SYSTEM_RSDP_ADDR = NULL;
XSDT_DESCRIPTOR *SYSTEM_XSDT_ADDR = NULL;
FADT_DESCRIPTOR *SYSTEM_FADT_ADDR = NULL;

ACPI_SDT_HEADER *find_FADT(){
    //在XSDT中查找FADT
    XSDT_DESCRIPTOR* XSDT = SYSTEM_XSDT_ADDR;
    if(memcmp(XSDT->Signature,"XSDT",4) == 0){
        size_t entry_count = (XSDT->Length - 28) / 8;//计算指针数
        //遍历每个指针，查找“FACP”
        for (size_t i = 0; i < entry_count; i++) {
            uint64_t table_phys_addr = XSDT->Entry[i] >> 32;
            ACPI_SDT_HEADER* table_header = (ACPI_SDT_HEADER*)table_phys_addr;
            if (!table_header) continue; //映射失败，跳过
            #ifdef debug
            printf("%x:%s\n",table_phys_addr,table_header->Signature);
            #endif
            // 检查签名是否为 “FACP”
            if (memcmp(table_header, "FACP", 4) == 0) {
                return table_header; //返回FADT表头指针
            }
        }
    }
    //找不到就在RSDT中查找FADT表
    RSDT_DESCRIPTOR* RSDT = (RSDT_DESCRIPTOR*)(uint64_t)SYSTEM_RSDP_ADDR->RsdtAddress;
    if(memcmp(RSDT->Signature,"RSDT",4) == 0){
        size_t entry_count = (RSDT->Length - 32) / 4;//计算指针数
        //遍历每个指针，查找“FACP”
        for (size_t i = 0; i < entry_count; i++) {
            uint32_t table_phys_addr = RSDT->Entry[i];
            ACPI_SDT_HEADER* table_header = (ACPI_SDT_HEADER*)(uint64_t)table_phys_addr;
            if (!table_header) continue; //映射失败，跳过
            #ifdef debug
            printf("%x:%s\n",table_phys_addr,table_header->Signature);
            #endif
            // 检查签名是否为 “FACP”
            if (memcmp(table_header, "FACP", 4) == 0) {
                return table_header; //返回FADT表头指针
            }
        }
    }
    return NULL;

}

//全局ACPI
_Bool init_ACPI(RSDP_DESCRIPTOR* RSDP){
    if(!RSDP) return false;
    if(RSDP->Revision == 0)return false;
    SYSTEM_RSDP_ADDR = RSDP;
    SYSTEM_XSDT_ADDR = (XSDT_DESCRIPTOR*)SYSTEM_RSDP_ADDR->XsdtAddress;
    SYSTEM_FADT_ADDR = (FADT_DESCRIPTOR*)find_FADT();
    if(!SYSTEM_XSDT_ADDR || !SYSTEM_FADT_ADDR)return false;
    return true;
}
