/**
 * OS003 硬件驱动-ATA（IDE）接口硬盘驱动
 * 2026/7/10 Liu Chunyi
 */

#ifndef _DRIVES_ATA_H_
#define _DRIVES_ATA_H_

#include <klib.h>

//ATA 标准端口（Primary 通道）
#define ATA_PRIMARY_DATA          0x1F0   // 数据端口 (16-bit)
#define ATA_PRIMARY_ERROR         0x1F1   // 错误/特性
#define ATA_PRIMARY_SECTOR_COUNT  0x1F2   // 扇区计数
#define ATA_PRIMARY_LBA_LO        0x1F3   // LBA 低字节
#define ATA_PRIMARY_LBA_MID       0x1F4   // LBA 中字节
#define ATA_PRIMARY_LBA_HI        0x1F5   // LBA 高字节
#define ATA_PRIMARY_DRIVE         0x1F6   // 驱动器/磁头
#define ATA_PRIMARY_COMMAND       0x1F7   // 命令（写）/ 状态（读）
#define ATA_PRIMARY_ALT_STATUS    0x3F6   // 备用状态/控制

//ATA 标准端口（Secondary 通道）
#define ATA_SECONDARY_DATA         0x170
#define ATA_SECONDARY_ERROR        0x171
#define ATA_SECONDARY_SECTOR_COUNT 0x172
#define ATA_SECONDARY_LBA_LO       0x173
#define ATA_SECONDARY_LBA_MID      0x174
#define ATA_SECONDARY_LBA_HI       0x175
#define ATA_SECONDARY_DRIVE        0x176
#define ATA_SECONDARY_COMMAND      0x177
#define ATA_SECONDARY_ALT_STATUS   0x376

//ATA命令代码
#define ATA_CMD_IDENTIFY         0xEC   // IDENTIFY 命令
#define ATA_CMD_READ_SECTORS     0x20   // 读扇区 (PIO, LBA28)
#define ATA_CMD_READ_SECTORS_EXT 0x24   // 读扇区 (PIO, LBA48)
#define ATA_CMD_WRITE_SECTORS    0x30   // 写扇区 (PIO, LBA28)
#define ATA_CMD_WRITE_SECTORS_EXT 0x34   // 写扇区 (PIO, LBA48)
#define ATA_CMD_READ_DMA         0xC8   // 读扇区 (DMA)
#define ATA_CMD_FLUSH_CACHE      0xE7   // 刷新缓存
#define ATA_CMD_SET_FEATURES     0xEF   // 设置特性

//驱动器选择
#define ATA_DRIVE_MASTER         0xA0   // 选主盘
#define ATA_DRIVE_SLAVE          0xB0   // 选从盘

//ATA 状态寄存器位
#define ATA_SR_BSY     0x80   // Busy
#define ATA_SR_DRDY    0x40   // Drive Ready
#define ATA_SR_DF      0x20   // Drive Fault
#define ATA_SR_DSC     0x10   // Seek Complete
#define ATA_SR_DRQ     0x08   // Data Request Ready
#define ATA_SR_CORR    0x04   // Corrected Data
#define ATA_SR_IDX     0x02   // Index
#define ATA_SR_ERR     0x01   // Error

//IDENTIFY 数据中关键Word的偏移
#define ATA_IDENT_WORDS          256    // IDENTIFY 数据总 word 数
#define ATA_IDENT_TYPE           0      // word 0: 设备类型
#define ATA_IDENT_CYLINDERS      1      // word 1-2: 柱面数（仅 CHS）
#define ATA_IDENT_HEADS          3      // word 3: 磁头数
#define ATA_IDENT_SECTORS        6      // word 6: 每磁道扇区数
#define ATA_IDENT_SERIAL         10     // word 10-19: 序列号 (10 words = 20 bytes)
#define ATA_IDENT_FW_REV         23     // word 23-26: 固件版本 (4 words = 8 bytes)
#define ATA_IDENT_MODEL          27     // word 27-46: 型号 (20 words = 40 bytes)
#define ATA_IDENT_CAPABILITIES   49     // word 49: 能力
#define ATA_IDENT_FIELD_VALID    53     // word 53: 字段有效性
#define ATA_IDENT_LBA28_SECTORS  60     // word 60-61: LBA28 总扇区数
#define ATA_IDENT_LBA48_SECTORS  100    // word 100-103: LBA48 总扇区数

//设备类型位（word 0）
#define ATA_TYPE_ATA  0x0000       // bit 15:0, bit 14:0 = ATA 盘
#define ATA_TYPE_ATAPI (1 << 14 | 1 << 15)   // bit 15=1, bit 14=1 = ATAPI (光驱/磁带)

//ATA 通道结构
typedef struct {
    uint16_t data_port;       // 数据端口
    uint16_t error_port;      // 错误/特性端口
    uint16_t sector_count;    // 扇区计数端口
    uint16_t lba_lo;          // LBA 低
    uint16_t lba_mid;         // LBA 中
    uint16_t lba_hi;          // LBA 高
    uint16_t drive_port;      // 驱动器/磁头
    uint16_t command_port;    // 命令/状态
    uint16_t alt_status_port; // 备用状态
} ata_channel_t;

//硬盘信息结构体
typedef struct {
    _Bool     present;            // 是否存在
    _Bool     is_master;          // 是主盘还是从盘
    _Bool     is_atapi;           // 是 ATAPI 设备（光驱）
    _Bool     lba48_supported;    // 支持 LBA48
    char      model[41];          // 型号字符串 (40 字节 + 结束符)
    char      serial[21];         // 序列号
    uint64_t  total_sectors;      // 总扇区数
    uint64_t  size_in_mb;         // 以 MB 为单位的大小
} ata_drive_t;

//IDE控制器找到的硬盘
typedef struct {
    _Bool      controller_found;  // 是否找到 IDE 控制器
    uint16_t   vendor_id;         // IDE 控制器的 VendorID
    uint16_t   device_id;         // IDE 控制器的 DeviceID
    ata_drive_t drives[4];        // Primary Master/Slave, Secondary Master/Slave
    int        drive_count;       // 实际检测到的硬盘数（不含光驱）
} ata_controller_info_t;

//硬盘描述符
typedef struct {
    _Bool is_primary;           
    _Bool is_master;
    ata_channel_t channel;
} ata_disk_t;

/**
 * 初始化 ATA 通道结构体
 * @param channel 目标通道指针
 * @param is_primary 1=Primary, 0=Secondary
 */
void ATA_Init_Channel(ata_channel_t *channel, _Bool is_primary);
/**
 * 等待 ATA 总线不忙
 * @param channel ATA 通道
 * @param timeout 超时毫秒数
 * @return 0=成功, -1=超时
 */
int ATA_Wait_Busy(ata_channel_t *channel, uint32_t timeout);
/**
 * 等待 ATA 数据就绪（DRQ 置位）
 * @param channel ATA 通道
 * @param timeout 超时毫秒数
 * @return 0=数据就绪, -1=超时/错误/无设备
 */
int ATA_Wait_DRP(ata_channel_t *channel, uint32_t timeout);
/**
 * 向指定通道发送 IDENTIFY 命令并解析结果
 * @param channel  ATA 通道
 * @param drive    ata_drive_t 结构体指针（接收结果）
 * @param is_master 1=主盘, 0=从盘
 * @return 0=识别成功, -1=无设备, -2=超时, -3=ATAPI 设备
 */
int ATA_Identify_Drive(ata_channel_t *channel, ata_drive_t *drive, _Bool is_master);
/**
 * 扫描所有 IDE 通道（Primary + Secondary）检测硬盘
 * @param info ata_controller_info_t 指针（接收结果）
 */
void ata_scan_all(ata_controller_info_t *info);
/**
 * 打印检测到的硬盘信息
 * @param info 硬盘检测结果
 */
void ATAPrintInfo(ata_controller_info_t *info);

/**
 * 使用 LBA28 (28-bit) 发送读取命令
 * @param channel       ATA 通道
 * @param is_master     驱动器选择 (0=主盘, 1=从盘)
 * @param lba           LBA 起始扇区号 (28-bit, 最大 0x0FFFFFFF)
 * @param sector_count  要读取的扇区数 (最大 256, 其中 0=256)
 */
int ATA_Read_LBA28_cmd(ata_channel_t *channel, _Bool is_master, uint32_t lba,uint16_t sector_count);

/**
 * 使用 LBA48 (48-bit) 发送读取命令
 * @param channel       ATA 通道
 * @param is_master     驱动器选择 (0=主盘, 1=从盘)
 * @param lba           LBA 起始扇区号 (48-bit)
 * @param sector_count  要读取的扇区数 (最大 65536, 其中 0=65536)
 */
int ATA_Read_LBA48_cmd(ata_channel_t *channel, _Bool is_master, uint64_t lba,uint16_t sector_count);

/**
 * 自动选择 LBA28/LBA48，读取扇区
 * @param channel       ATA 通道
 * @param is_master     驱动器选择 (0=主盘, 1=从盘)
 * @param lba           LBA 起始扇区号
 * @param sector_count  要读取的扇区数
 * @param buffer        目标缓冲区
 * @return 成功读取的扇区数, 负值=出错
 */
int ATA_Read(ata_channel_t *channel, _Bool is_master, uint64_t lba,uint16_t sector_count, void *buffer);

/**
 * 发送 LBA28 PIO 写入命令
 * @param channel       ATA 通道
 * @param is_master     驱动器选择 (0=主盘, 1=从盘)
 * @param lba           LBA 起始扇区号 (48-bit)
 * @param sector_count  要读取的扇区数 (最大 65536, 其中 0=65536)
 */
int ATA_Write_LBA28_cmd(ata_channel_t *channel, _Bool is_master, uint32_t lba, uint16_t sector_count);

/**
 * 发送 LBA48 PIO 写入命令
 * @param channel       ATA 通道
 * @param is_master     驱动器选择 (0=主盘, 1=从盘)
 * @param lba           LBA 起始扇区号 (48-bit)
 * @param sector_count  要读取的扇区数 (最大 65536, 其中 0=65536)
 */
int ATA_Write_LBA48_cmd(ata_channel_t *channel, _Bool is_master, uint64_t lba, uint16_t sector_count);

/**
 * 自动选择 LBA28/LBA48 写入扇区
 * @param channel       ATA 通道
 * @param is_master     驱动器选择 (0=主盘, 1=从盘)
 * @param lba           LBA 起始扇区号
 * @param sector_count  要读取的扇区数
 * @param buffer        数据缓冲区
 * @return 成功写入取的扇区数, 负值=出错
 */
int ATA_Write(ata_channel_t *channel, _Bool is_master, uint64_t lba, uint16_t sector_count, const void *buffer);

/**
 * 刷新 ATA 磁盘写缓存（确保数据真正写入磁盘）
 * @param channel       ATA 通道
 * @param is_master     驱动器选择 (0=主盘, 1=从盘)
 */
void ATA_Flush_Cache(ata_channel_t *channel, _Bool is_master);

ata_disk_t FindFirstATADisk();//寻找第一个硬盘

extern ata_controller_info_t gATAInfo;//全局ATA扫描结果

#endif