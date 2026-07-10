#ifndef _FS_FAT32_H_
#define _FS_FAT32_H_

#include <klib.h>
#include <drives/ata.h>
#include <drives/gpt.h>

// FAT32 完整信息结构体
typedef struct {
    //从DBR BPB解析
    uint16_t bytes_per_sector;       // 每扇区字节数（通常 512）
    uint8_t  sectors_per_cluster;    // 每簇扇区数（通常 8 或 16）
    uint16_t reserved_sectors;       // 保留扇区数（FAT 表前的扇区数）
    uint8_t  num_fats;               // FAT 表数量（通常 2）
    uint32_t sectors_per_fat;        // 每个 FAT 表占多少扇区（FAT32: 32位字段）
    uint32_t root_cluster;           // 根目录的起始簇号（FAT32: 通常为 2）
    uint16_t fsinfo_sector;          // FSInfo 扇区相对偏移（通常为 1）
    uint16_t backup_boot_sector;     // 备份引导扇区偏移（通常为 6）
    //计算得到
    uint32_t fat_region_lba;         // FAT 表区域起始 LBA（相对分区）
    uint32_t data_region_lba;        // 数据区起始 LBA（相对分区）
    uint32_t total_sectors;          // 分区总扇区数
    uint32_t total_clusters;         // 总簇数
    //从FSInfo扇区读取
    uint32_t free_clusters;          // 空闲簇数（0xFFFFFFFF = 未知）
    uint32_t next_free_cluster;      // 下一个空闲簇提示
    //扩展BPB信息
    char     volume_label[12];       // 卷标（11 字节 + 结尾 \0）
    uint32_t volume_id;              // 卷序列号
} fat32_info_t;

//FAT32目录项结构（短文件名 SFN）
typedef struct __attribute__((packed)) {
    uint8_t  name[11];           // 8.3 文件名（8字节名+3字节扩展名）
    uint8_t  attr;               // 文件属性
    uint8_t  nt_reserved;        // WinNT 保留（大小写标志）
    uint8_t  creation_tenths;    // 创建时间的毫秒部分
    uint16_t creation_time;      // 创建时间
    uint16_t creation_date;      // 创建日期
    uint16_t last_access_date;   // 上次访问日期
    uint16_t cluster_high;       // 起始簇号高16位（FAT32）
    uint16_t last_write_time;    // 最后写入时间
    uint16_t last_write_date;    // 最后写入日期
    uint16_t cluster_low;        // 起始簇号低16位
    uint32_t file_size;          // 文件大小（字节）
} fat32_dir_entry_t;

//文件属性位
#define FAT32_ATTR_READ_ONLY  0x01
#define FAT32_ATTR_HIDDEN     0x02
#define FAT32_ATTR_SYSTEM     0x04
#define FAT32_ATTR_VOLUME_ID  0x08
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20
#define FAT32_ATTR_LFN        0x0F  //长文件名项

//FAT表项特殊值
#define FAT32_ENTRY_FREE      0x00000000    //空闲簇
#define FAT32_ENTRY_EOF       0x0FFFFFF8    //簇链结束 (≥0x0FFFFFF8)
#define FAT32_ENTRY_BAD       0x0FFFFFF7    //坏簇

//长文件名项结构
typedef struct __attribute__((packed)) {
    uint8_t  order;              // 序号（0x40=最后一次，0xE5=已删）
    uint16_t name1[5];           // 字符 1-5 (UTF-16LE)
    uint8_t  attr;               // 必须=0x0F
    uint8_t  type;               // 0=主项
    uint8_t  checksum;           // 短文件名校验和
    uint16_t name2[6];           // 字符 6-11
    uint16_t first_cluster;      // 必须=0
    uint16_t name3[2];           // 字符 12-13
} fat32_lfn_entry_t;

#define FAT32_MAX_NAME 260//带长文件名的目录列表项

typedef struct {
    fat32_dir_entry_t entry;//原始短文件名目录项
    char display_name[FAT32_MAX_NAME];//显示用的文件名
} fat32_list_entry_t;

/**
 * 从FAT表中读取给定簇号的下一个簇号
 * @param Info      FAT32 分区信息
 * @param Disk      硬盘描述符
 * @param Partition 分区描述符
 * @param cluster   当前簇号
 * @param next      输出：下一个簇号（FAT32_ENTRY_EOF 表示链结束）
 * @return 0=成功, 非0=失败
 */
STATUS FAT32_NextCluster(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition,uint32_t cluster, uint32_t *next);

/**
 * 将逻辑簇号转换为分区的绝对LBA
 * @param Info    FAT32 分区信息
 * @param cluster 簇号
 * @return 该簇第一个扇区的分区绝对 LBA
 */
uint64_t FAT32_ClusterToLBA(fat32_info_t *Info, uint32_t cluster);

/**
 * 读取一个簇的数据
 * @param Info      FAT32分区信息
 * @param Disk      硬盘描述符
 * @param Partition 分区描述符
 * @param cluster   簇号
 * @param buffer    目标缓冲区（至少 cluster_size 字节）
 * @return 0=成功, 非0=失败
 */
STATUS FAT32_ReadCluster(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition,uint32_t cluster, void *buffer);

/**
 * 写入一个簇的数据
 * @param Info      FAT32分区信息
 * @param Disk      硬盘描述符
 * @param Partition 分区描述符
 * @param cluster   簇号
 * @param buffer    数据缓冲区（至少 cluster_size 字节）
 * @return 0=成功, 非0=失败
 */
STATUS FAT32_WriteCluster(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition,uint32_t cluster, void *buffer);

/**
 * 在指定目录簇中查找文件/子目录
 * @param Info      FAT32分区信息
 * @param Disk      硬盘描述符
 * @param Partition 分区描述符
 * @param dir_cluster 目录的起始簇号（根目录用 Info->root_cluster）
 * @param filename  要查找的文件名（仅 8.3 短文件名，如 "KERNEL  ELF"）
 * @param entry     输出：找到的目录项
 * @return 0=找到, 1=未找到, 其他=错误
 */
STATUS FAT32_FindEntry(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition,uint32_t dir_cluster, const char *filename,fat32_dir_entry_t *entry);

/**
 * 读取一个文件的全部数据（通过簇链遍历）
 * @param Info      FAT32分区信息
 * @param Disk      硬盘描述符
 * @param Partition 分区描述符
 * @param entry     文件目录项
 * @param buffer    目标缓冲区（需足够大）
 * @return 读取的字节数, 0=错误
 */
uint32_t FAT32_ReadFile(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition,fat32_dir_entry_t *entry, void *buffer);

/**
 * 获取 FAT32 分区完整信息
 * @param Disk      硬盘描述符指针
 * @param Partition 分区描述符指针
 * @param Info      输出参数，接收解析后的 FAT32 信息
 * @return 0=成功，1=DBR 无效，2=FSInfo 无效
 */
STATUS FAT32_GetInfo(ata_disk_t *Disk, gpt_partition_entry_t *Partition, fat32_info_t *Info);

void name_to_83(const char *name, uint8_t *out_83);

/**
 * 计算短文件名（11 字节）的 LFN 校验和
 * @param sfn_name 11 字节的短文件名
 * @return 校验和（每个 LFN 项里存的就是这个值，用来匹配）
 */
uint8_t fat32_lfn_checksum(const uint8_t *sfn_name);

/**
 * 按文件名查找目录项（支持长文件名 + 短文件名双模式）
 * 先尝试按长文件名匹配，找不到再按 8.3 匹配
 * @param Info      分区信息
 * @param Disk      硬盘描述符
 * @param Partition 分区描述符
 * @param dir_cluster 目录起始簇号
 * @param long_name 要查找的文件名（如 "Long File Name.txt"）
 * @param entry     输出：找到的短文件名目录项（含簇号、大小）
 * @return 0=找到，1=未找到，其他=错误
 */
STATUS FAT32_FindEntryAny(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition,uint32_t dir_cluster, const char *long_name,fat32_dir_entry_t *entry);

/**
 * 列出指定目录
 * @param Info      分区信息
 * @param Disk      硬盘描述符
 * @param Partition 分区描述符
 * @param dir_cluster 目录起始簇号
 * @param count     文件/目录数量指针
 * @param out_pages 目录项列表所占的页数
 * @return 目录项列表
 */
fat32_list_entry_t* FAT32_ListDirectory(fat32_info_t *Info,ata_disk_t *Disk,gpt_partition_entry_t *Partition,uint32_t dir_cluster,int *count,int *out_pages);

/**
 * 文件写入（覆盖）
 * @param Info        FAT32分区信息
 * @param Disk        硬盘描述符
 * @param Partition   分区描述符
 * @param dir_cluster 文件所在目录的起始簇号（根目录用 Info->root_cluster）
 * @param entry       文件目录项（传入时会更新 file_size 字段）
 * @param data        要写入的数据缓冲区
 * @param size        要写入的字节数
 * @return 实际写入的字节数，0=失败
 */
uint32_t FAT32_WriteFile(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition, uint32_t dir_cluster,fat32_dir_entry_t *entry, const void *data, uint32_t size);

/**
 * 文件写入（追加模式）
 * @param Info        FAT32分区信息
 * @param Disk        硬盘描述符
 * @param Partition   分区描述符
 * @param dir_cluster 文件所在目录的起始簇号（根目录用 Info->root_cluster）
 * @param entry       文件目录项（传入时会更新 file_size 字段）
 * @param data        要写入的数据缓冲区
 * @param size        要写入的字节数
 * @return 实际写入的字节数，0=失败
 */
uint32_t FAT32_AppendFile(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition, uint32_t dir_cluster,fat32_dir_entry_t *entry, const void *data, uint32_t size);

/**
 * 修改文件名（重命名，保留LFN长文件名）
 * 如果原文件有长文件名且新旧名字需要相同数量的LFN条目，
 * 则原地更新LFN条目内容（保留校验和一致性）；
 * 否则删除原LFN条目，退化为仅更新8.3短文件名。
 * @param Info       FAT32分区信息
 * @param Disk       硬盘描述符
 * @param Partition  分区描述符
 * @param dir_cluster 文件所在目录的起始簇号
 * @param old_name   原文件名（支持长文件名）
 * @param new_name   新文件名（支持长文件名）
 * @return 0=成功, 1=未找到, 2=读错误, 3=写错误, 4=新文件名已存在
 */
STATUS FAT32_RenameFile(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, const char *old_name, const char *new_name);

/**
 * 创建空文件
 * @param Info       FAT32分区信息
 * @param Disk       硬盘描述符
 * @param Partition  分区描述符
 * @param dir_cluster 父目录起始簇号（根目录用 Info->root_cluster）
 * @param filename   要创建的文件名（支持长文件名）
 * @return 0=成功, 1=文件已存在, 2=读错误, 3=写错误, 4=目录已满, 5=分配失败
 */
STATUS FAT32_CreateFile(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, const char *filename);

/**
 * 创建空目录
 * @param Info       FAT32分区信息
 * @param Disk       硬盘描述符
 * @param Partition  分区描述符
 * @param dir_cluster 父目录起始簇号（根目录用 Info->root_cluster）
 * @param dirname   要创建的文件名（支持长文件名）
 * @return 0=成功, 1=文件已存在, 2=读错误, 3=写错误, 4=目录已满, 5=分配失败
 */
STATUS FAT32_CreateDir(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, const char *dirname);

/**
 * 删除空目录
 * @param Info        FAT32分区信息
 * @param Disk        硬盘描述符
 * @param Partition   分区描述符
 * @param dir_cluster 父目录起始簇号
 * @param dirname     要删除的目录名
 * @return 0=成功, 1=未找到, 2=读错误, 3=写错误, 4=不是目录, 5=目录非空
 */
STATUS FAT32_RemoveDir(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, const char *dirname);

/**
 * 删除文件
 * 释放数据簇链，并在父目录中标记为已删除
 * @param Info        FAT32分区信息
 * @param Disk        硬盘描述符
 * @param Partition   分区描述符
 * @param dir_cluster 文件所在目录的起始簇号
 * @param filename    要删除的文件名
 * @return 0=成功, 1=未找到, 2=读错误, 3=写错误, 4=是目录不是文件
 */
STATUS FAT32_DeleteFile(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, const char *filename);

#endif