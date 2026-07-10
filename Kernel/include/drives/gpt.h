/**
 * OS003 GPT 分区表识别库
 * 2026/7/10 Liu Chunyi
 */

#ifndef _DRIVERS_GPT_H_
#define _DRIVERS_GPT_H_

#include <klib.h>
#include "pci.h"
#include "ata.h"

//GPT头结构定义（小端序）
typedef struct {
    uint8_t  signature[8];      // 0x00: "EFI PART"
    uint32_t revision;          // 0x08: 版本号
    uint32_t header_size;       // 0x0C: 头大小（通常92字节）
    uint32_t header_crc32;      // 0x10: CRC32校验值
    uint32_t reserved;          // 0x14: 保留
    uint64_t my_lba;            // 0x18: 当前LBA（应为1）
    uint64_t alternate_lba;     // 0x20: 备份GPT头LBA
    uint64_t first_usable_lba;  // 0x28: 首个可用LBA
    uint64_t last_usable_lba;   // 0x30: 最后可用LBA
    uint64_t  disk_guid1;       // 0x38: 磁盘GUID(前半段)
    uint64_t  disk_guid2;       // 0x40: 磁盘GUID(后半段)
    uint64_t partition_entry_lba; // 0x48: 分区表项起始LBA
    uint32_t num_partition_entries; // 0x50: 分区表项数量
    uint32_t size_partition_entry;  // 0x54: 每项大小（字节）
    uint32_t partition_entry_array_crc32; // 0x58: 分区表项CRC32
    //后续还有保留字段，但通常我们只用到上述部分
} __attribute__((packed)) gpt_header_t;

// GPT分区表项结构（128字节）
typedef struct {
    uint8_t partition_type_guid[16];        // 0x00-0x0F: 分区类型GUID (16字节)
    uint8_t unique_partition_guid[16];      // 0x10-0x1F: 该分区唯一的GUID (16字节)
    uint64_t starting_lba;                  // 0x20-0x27: 起始LBA (8字节，小端序)
    uint64_t ending_lba;                    // 0x28-0x2F: 结束LBA (8字节，小端序，包含此LBA)
    uint64_t attributes;                    // 0x30-0x37: 属性标志 (8字节)
    uint16_t partition_name[36];            // 0x38-0x77: 分区名称 (72字节，UTF-16LE编码，最多36个字符)
} __attribute__((packed)) gpt_partition_entry_t;

extern const uint8_t ESP_GUID[16];

/**
 * 检查GPT硬盘MBR保护性扇区
 * @param buffer LBA1扇区
 */
_Bool Check_MBR(uint8_t* buffer);
/**
 * 检查GPT硬盘表头
 * @param buffer LBA1扇区
 * @return 1:签名不正确 2：头大小不合法 3：版本不支持 0：正常
 */
STATUS Check_GPT_Header(uint8_t* buffer);
/**
 * 寻找分区
 * @param channel ATA通道
 * @param is_master 是否主盘
 * @param type_guid 分区类型GUID
 * @param result 分区表项
 * @return 1:参数错误 2:MBR错误 3:GPT表头错误 4:未时别到目标分区 0:成功 -1:扇区读取错误 -2:临时缓冲区申请错误
 */
STATUS FindPartition(ata_channel_t *channel, _Bool is_master,const uint8_t type_guid[16],gpt_partition_entry_t* result);

#endif