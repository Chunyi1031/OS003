#include <drives/gpt.h>
#include <Print.h>
#include <Memory.h>

const uint8_t ESP_GUID[16] = {
    0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
    0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
};

_Bool Check_MBR(uint8_t* buffer){
    if(buffer[510] != 0x55 || buffer[511] != 0xAA || buffer[450] != 0xEE)return false;
    return true;
}

STATUS Check_GPT_Header(uint8_t* buffer){
    const gpt_header_t *header = (const gpt_header_t *)buffer;
    //验证签名
    if (memcmp(header->signature, "EFI PART", 8) != 0) {
        return 1;
    }
    //验证头大小是否合法
    uint32_t header_size = header->header_size;
    if (header_size < 92 || header_size > 512) {
        return 2;
    }
    //验证版本号
    if (header->revision != 0x00010000) {
        return 3;
    }
    return 0;
}

STATUS FindPartition(ata_channel_t *channel, _Bool is_master,const uint8_t type_guid[16],gpt_partition_entry_t* result){
    if(!result)return 1;
    uint8_t buffer[512];//单扇区缓冲区
    //检查保护性MBR
    int rr = ATA_Read(channel,is_master,0,1,buffer);
    if(!rr)return -1;
    if(!Check_MBR(buffer)){
        out_error("MBR is invalid");
        return 2;
    }
    //检查GPT表头
    rr = ATA_Read(channel,is_master,1,1,buffer);
    if(!rr)return -1;
    if(Check_GPT_Header(buffer) != 0){
        out_error("GPT Header is invalid");
        return 3;
    }
    gpt_header_t* gpt_header = (gpt_header_t*)buffer;
    void* partition_entry = Vmm_Malloc(gpt_header->num_partition_entries * gpt_header->size_partition_entry);//分配分区表临时缓冲区
    if(!partition_entry)return -2;
    rr = ATA_Read(channel,is_master,gpt_header->partition_entry_lba,(gpt_header->num_partition_entries * gpt_header->size_partition_entry / 512),partition_entry);//读取分区表到缓冲区
    if(!rr){Vmm_Free(partition_entry,gpt_header->num_partition_entries * gpt_header->size_partition_entry);return -1;}
    //遍历所有表项
    for(int i = 0;i < gpt_header->num_partition_entries;i++){
        gpt_partition_entry_t* entry = (gpt_partition_entry_t*)((uint64_t)partition_entry + i * gpt_header->size_partition_entry);
        if(memcmp(entry->partition_type_guid,type_guid,16) == 0){
            memcpy(result,entry,sizeof(gpt_partition_entry_t));
            Vmm_Free(partition_entry,gpt_header->num_partition_entries * gpt_header->size_partition_entry);//回收分区表临时缓冲区
            return 0;
        }
    }
    Vmm_Free(partition_entry,gpt_header->num_partition_entries * gpt_header->size_partition_entry);//回收分区表临时缓冲区
    return 4;//找不到
}
