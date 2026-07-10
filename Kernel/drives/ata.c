#include <drives/ata.h>
#include <drives/pci.h>
#include <Print.h>

//初始化ATA通道的端口号
void ATA_Init_Channel(ata_channel_t *channel, _Bool is_primary) {
    if (is_primary) {
        channel->data_port       = ATA_PRIMARY_DATA;
        channel->error_port      = ATA_PRIMARY_ERROR;
        channel->sector_count    = ATA_PRIMARY_SECTOR_COUNT;
        channel->lba_lo          = ATA_PRIMARY_LBA_LO;
        channel->lba_mid         = ATA_PRIMARY_LBA_MID;
        channel->lba_hi          = ATA_PRIMARY_LBA_HI;
        channel->drive_port      = ATA_PRIMARY_DRIVE;
        channel->command_port    = ATA_PRIMARY_COMMAND;
        channel->alt_status_port = ATA_PRIMARY_ALT_STATUS;
    } else {
        channel->data_port       = ATA_SECONDARY_DATA;
        channel->error_port      = ATA_SECONDARY_ERROR;
        channel->sector_count    = ATA_SECONDARY_SECTOR_COUNT;
        channel->lba_lo          = ATA_SECONDARY_LBA_LO;
        channel->lba_mid         = ATA_SECONDARY_LBA_MID;
        channel->lba_hi          = ATA_SECONDARY_LBA_HI;
        channel->drive_port      = ATA_SECONDARY_DRIVE;
        channel->command_port    = ATA_SECONDARY_COMMAND;
        channel->alt_status_port = ATA_SECONDARY_ALT_STATUS;
    }
}

int ATA_Wait_Busy(ata_channel_t *channel, uint32_t timeout) {
    uint64_t timeout_cycles = (uint64_t)timeout * (SYSTEM_CPU_Frequency / 1000);
    uint64_t start_tsc = rdtsc();
    //读交替状态端口（不触发中断），直到 BSY=0
    while (inb(channel->alt_status_port) & ATA_SR_BSY) {
        //超时
        if ((rdtsc() - start_tsc) > timeout_cycles) {
            return -1;
        }
        asm("pause");
    }
    return 0;
}

int ATA_Wait_DRP(ata_channel_t *channel, uint32_t timeout) {
    uint64_t timeout_cycles = (uint64_t)timeout * (SYSTEM_CPU_Frequency / 1000);
    uint64_t start_tsc = rdtsc();
    while (1) {
        uint8_t status = inb(channel->alt_status_port);
        //等待BSY清空
        if (status & ATA_SR_BSY) {
            if ((rdtsc() - start_tsc) > timeout_cycles) return -1; // 超时
            asm("pause");
            continue;
        }
        //检查是否有错误
        if (status & ATA_SR_ERR) {
            return -2;//设备错误或无设备
        }
        //BSY=0, ERR=0，检查DRQ
        if (status & ATA_SR_DRQ) {
            return 0;   // 数据就绪，可以读取了
        }
        //如果 BSY=0, ERR=0, DRQ=0，那么可能是设备不存在
        //等待一小段时间后检查超时
        if ((rdtsc() - start_tsc) > timeout_cycles) {
            return -3;//无设备响应
        }
        asm("pause");
    }
}

/*DeepSeek-V4-Flash*/
// ── 对指定通道的主盘/从盘执行 IDENTIFY ──
// 原理：
// 1. 选择驱动器（写 0xA0/0xB0 到 drive_port）
// 2. 等待 BSY=0
// 3. 写 0xEC (IDENTIFY 命令) 到 command_port
// 4. 等待 DRQ=1（或 ERR=1 表示无设备）
// 5. 用 inw(data_port) 循环 256 次，读取 512 字节
// 6. 解析数据
int ATA_Identify_Drive(ata_channel_t *channel, ata_drive_t *drive, _Bool is_master) {
    uint16_t buffer[ATA_IDENT_WORDS];  // 256 个 word = 512 字节
    uint8_t  drive_select;

    // 初始化结果
    memset(drive, 0, sizeof(ata_drive_t));
    drive->is_master = is_master;

    // 第一步：选择驱动器
    // LBA 模式下选择驱动器：bit 6=1(LBA), bit 5=1, bit 4=驱动器号, bits 3:0=LBA27:24
    // 这里只选驱动器，不设地址，所以 bits 3:0 = 0
    drive_select = is_master ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE;
    outb(channel->drive_port, drive_select);

    // 等待 10ms 让驱动器就绪
    delay_ms(10);

    // 第二步：等待 BSY 清空
    if (ATA_Wait_Busy(channel, 1000) != 0) {
        return -2;  // 超时
    }

    // 第三步：写 0 到特性、扇区计数、LBA 低中高寄存器
    // 这是 ATA 规范要求的：在 IDENTIFY 之前清除这些寄存器
    outb(channel->sector_count, 0);
    outb(channel->lba_lo, 0);
    outb(channel->lba_mid, 0);
    outb(channel->lba_hi, 0);

    // 第四步：发送 IDENTIFY 命令
    outb(channel->command_port, ATA_CMD_IDENTIFY);

    // 第五步：检查是否真的有设备
    // 如果端口没有连接设备，读取状态会立即反映
    // 发完命令后立即读状态，如果 BSY=0 且 DRQ=0 且 ERR=1 → 无设备
    delay_ms(10);  // 给设备一点反应时间
    uint8_t status = inb(channel->alt_status_port);
    if (status == 0) {
        return -1;  // 端口上没有设备
    }

    // 第六步：等待 DRQ=1（数据就绪）
    int drq_result = ATA_Wait_DRP(channel, 5000);

    // 特殊处理：对于模拟器（QEMU），即使无设备 ERR 也不会迅速置位
    // 我们给个备用判断：读 lba_mid 和 lba_hi，ATA 规范指定 IDENTIFY 完成后
    // 如果 lba_mid=0 且 lba_hi=0 → ATA 设备存在
    // 如果 lba_mid=0xFF 且 lba_hi=0xFF → ATAPI 设备存在
    // 如果其他值 → 无设备（端口浮空）
    uint8_t sig_mid = inb(channel->lba_mid);
    uint8_t sig_hi  = inb(channel->lba_hi);

    // 如果两个寄存器都是浮空值 0xFF，大概率无设备
    if ((sig_mid == 0xFF) && (sig_hi == 0xFF)) {
        return -1;  // 无设备
    }

    // 如果 drq 返回错误码
    if (drq_result < 0) {
        // 但可能是 ATAPI 设备（lba_mid=0x14, lba_hi=0xEB 是 ATAPI 签名）
        if ((sig_mid == 0x14) && (sig_hi == 0xEB)) {
            // ATAPI 设备（光驱等），我们暂时用另一种方式处理
            // 实际上 ATAPI 用 ATA_CMD_IDENTIFY_PACKET (0xA1)
            // 这里标记并返回
            drive->present = true;
            drive->is_atapi = true;
            strcpy(drive->model, "ATAPI Device");
            return -3;  // 标记为 ATAPI
        }
        return drq_result;
    }

    // 第七步：读取 256 个 word
    for (int i = 0; i < ATA_IDENT_WORDS; i++) {
        buffer[i] = inw(channel->data_port);
    }

    // 第八步：检查是否是 ATAPI 设备
    // word 0 的 bit 15=1 且 bit 14=1 → ATAPI
    if ((buffer[ATA_IDENT_TYPE] & ATA_TYPE_ATAPI) == ATA_TYPE_ATAPI) {
        drive->present  = true;
        drive->is_atapi = true;
        strcpy(drive->model, "ATAPI Device");
        return -3;
    }

    // 第九步：解析型号字符串 (word 27-46)
    // ATA 规范规定：型号字符串是 20 个 word = 40 字节，每个 word 的高位字节在前（大端序）
    // 我们需要交换每个 word 的两个字节后再复制
    for (int i = 0; i < 20; i++) {
        uint16_t w = buffer[ATA_IDENT_MODEL + i];
        drive->model[i * 2]     = (char)(w >> 8);     // 高位字节在前
        drive->model[i * 2 + 1] = (char)(w & 0xFF);   // 低位字节在后
    }
    drive->model[40] = '\0';  // 结束符

    // 去掉尾部空格（型号字符串通常是 40 字节，尾部空字节填充）
    for (int i = 39; i >= 0; i--) {
        if (drive->model[i] == ' ') {
            drive->model[i] = '\0';
        } else if (drive->model[i] != '\0') {
            break;
        }
    }

    // 第十步：解析序列号 (word 10-19)，同样需要字节交换
    for (int i = 0; i < 10; i++) {
        uint16_t w = buffer[ATA_IDENT_SERIAL + i];
        drive->serial[i * 2]     = (char)(w >> 8);
        drive->serial[i * 2 + 1] = (char)(w & 0xFF);
    }
    drive->serial[20] = '\0';

    // 第十一步：计算总扇区数
    // word 83 bit 10 = LBA48 支持标志
    if (buffer[83] & (1 << 10)) {
        // 支持 LBA48 → 使用 word 100-103（64 位扇区数）
        drive->lba48_supported = true;
        drive->total_sectors = (uint64_t)buffer[100] |
                              ((uint64_t)buffer[101] << 16) |
                              ((uint64_t)buffer[102] << 32) |
                              ((uint64_t)buffer[103] << 48);
    } else {
        // 仅支持 LBA28 → 使用 word 60-61（28 位，最大 128GB）
        drive->lba48_supported = false;
        drive->total_sectors = (uint32_t)buffer[60] |
                              ((uint32_t)buffer[61] << 16);
    }

    // 计算 MB 数（每个扇区 512 字节）
    drive->size_in_mb = drive->total_sectors / 2048;  // 512*1024 = 2048 扇区/MB
    drive->present = true;

    return 0;  // 成功
}

// ── 扫描所有 IDE 通道 ──
// 扫描 Primary 的 Master/Slave + Secondary 的 Master/Slave = 最多 4 个设备
void ata_scan_all(ata_controller_info_t *info) {
    ata_channel_t primary, secondary;

    memset(info, 0, sizeof(ata_controller_info_t));

    // 初始化两个通道的端口号
    ATA_Init_Channel(&primary, true);   // Primary 通道
    ATA_Init_Channel(&secondary, false); // Secondary 通道

    // 扫描 Primary Master
    int result = ATA_Identify_Drive(&primary, &info->drives[0], 1);
    if (result == 0) {
        info->drive_count++;
    } else if (result == -3) {
        // ATAPI 设备，不计入硬盘数但标记存在
        info->drives[0].present = true;
    }

    // 扫描 Primary Slave
    result = ATA_Identify_Drive(&primary, &info->drives[1], 0);
    if (result == 0) {
        info->drive_count++;
    } else if (result == -3) {
        info->drives[1].present = true;
    }

    // 扫描 Secondary Master
    result = ATA_Identify_Drive(&secondary, &info->drives[2], 1);
    if (result == 0) {
        info->drive_count++;
    } else if (result == -3) {
        info->drives[2].present = true;
    }

    // 扫描 Secondary Slave
    result = ATA_Identify_Drive(&secondary, &info->drives[3], 0);
    if (result == 0) {
        info->drive_count++;
    } else if (result == -3) {
        info->drives[3].present = true;
    }
}

// ── 打印硬盘信息 ──
void ATAPrintInfo(ata_controller_info_t *info) {
    const char *slot_names[4] = {
        "Primary   Master",
        "Primary   Slave ",
        "Secondary Master",
        "Secondary Slave "
    };

    if (info->drive_count == 0) {
        // 检查是否有 ATAPI 设备
        _Bool has_atapi = false;
        for (int i = 0; i < 4; i++) {
            if (info->drives[i].present && info->drives[i].is_atapi) {
                has_atapi = true;
                break;
            }
        }
        if (!has_atapi) {
            print("  No drives detected.\n", COLOR_YELLOW);
        }
    }

    for (int i = 0; i < 4; i++) {
        if (!info->drives[i].present) {
            printf("  [%s] (none)\n", slot_names[i]);
            continue;
        }

        if (info->drives[i].is_atapi) {
            printf("  [%s]", slot_names[i]);
            print(" ATAPI Device (CD/DVD)\n", COLOR_CYAN);
            continue;
        }

        // 正常硬盘
        printf("  [%s]\n", slot_names[i]);
        printf("    Model:  %s\n", info->drives[i].model);
        printf("    S/N:    %s\n", info->drives[i].serial);
        printf("    Size:   %d MB", (uint32_t)info->drives[i].size_in_mb);
        if (info->drives[i].lba48_supported) {
            print("  (LBA48)", COLOR_GREEN);
        } else {
            print("  (LBA28)", COLOR_YELLOW);
        }
        printf("\n");
    }
}
/*DeepSeek-V4-Flash-END*/

//LBA28
int ATA_Read_LBA28_cmd(ata_channel_t *channel, _Bool is_master, uint32_t lba,uint16_t sector_count) {
    uint8_t drive_byte;
    //参数校验
    if (sector_count == 0) sector_count = 256;
    if (lba > 0x0FFFFFFF) return -4;//LBA28超范围
    //选择驱动器
    drive_byte = 0xE0 | (is_master ? 0 : (1 << 4)) | ((lba >> 24) & 0x0F);
    outb(channel->drive_port, drive_byte);
    delay_ms(1);
    if (ATA_Wait_Busy(channel, 5000) != 0) return -2;//等待BSY=0
    //写扇区数和 LBA 地址（低24位）
    outb(channel->sector_count, (uint8_t)(sector_count & 0xFF));  // 扇区数
    outb(channel->lba_lo,       (uint8_t)(lba & 0xFF));            // LBA 7:0
    outb(channel->lba_mid,      (uint8_t)((lba >> 8) & 0xFF));     // LBA 15:8
    outb(channel->lba_hi,       (uint8_t)((lba >> 16) & 0xFF));    // LBA 23:16
    outb(channel->drive_port, drive_byte);//重新写一次驱动器选择
    outb(channel->command_port, ATA_CMD_READ_SECTORS);//发送READ SECTORS命令
    return 0;
}

//LBA48
int ATA_Read_LBA48_cmd(ata_channel_t *channel, _Bool is_master, uint64_t lba,uint16_t sector_count) {
    if (sector_count == 0) sector_count = (uint16_t)65536;
    if (lba > 0x0000FFFFFFFFFFFFULL) return -4;//LBA48超范围
    //设驱动器（bit 6=0 表示 LBA48）
    uint8_t drive_byte = 0x40 | (is_master ? 0 : (1 << 4));
    outb(channel->drive_port, drive_byte);
    delay_ms(1);
    if (ATA_Wait_Busy(channel, 5000) != 0) return -2;//等待 BSY=0
    //LBA48 特有,先写高 24 位再写低 24 位
    //写高24位
    outb(channel->sector_count, (uint8_t)((sector_count >> 8) & 0xFF));  // 扇区数高位
    outb(channel->lba_lo,       (uint8_t)((lba >> 24) & 0xFF));          // LBA 字节 3
    outb(channel->lba_mid,      (uint8_t)((lba >> 32) & 0xFF));          // LBA 字节 4
    outb(channel->lba_hi,       (uint8_t)((lba >> 40) & 0xFF));          // LBA 字节 5
    //写低24位
    outb(channel->sector_count, (uint8_t)(sector_count & 0xFF));         // 扇区数低位
    outb(channel->lba_lo,       (uint8_t)(lba & 0xFF));                  // LBA 字节 0
    outb(channel->lba_mid,      (uint8_t)((lba >> 8) & 0xFF));           // LBA 字节 1
    outb(channel->lba_hi,       (uint8_t)((lba >> 16) & 0xFF));          // LBA 字节 2
    outb(channel->command_port, ATA_CMD_READ_SECTORS_EXT);//发送读取命令
    return 0;
}

int Read_Sector(ata_channel_t *channel, _Bool is_master, uint64_t lba,uint16_t sector_count, void *buffer) {
    //逐扇区读取
    uint16_t sectors_done = 0;
    while (sectors_done < sector_count) {
        if (ATA_Wait_Busy(channel, 30000) != 0) return -(5 + sectors_done);
        if (inb(channel->alt_status_port) & ATA_SR_ERR) return -(10 + sectors_done);
        if (ATA_Wait_DRP(channel, 30000) != 0) return -(20 + sectors_done);
        uint16_t *word_buf = (uint16_t *)(buffer + sectors_done * 512);
        for (int i = 0; i < 256; i++) {
            word_buf[i] = inw(channel->data_port);
        }
        sectors_done++;
    }
    return sectors_done;
}

int ATA_Read(ata_channel_t *channel, _Bool is_master, uint64_t lba,uint16_t sector_count, void *buffer) {
    int result;
    //如果 LBA < 2^28 且只读少量扇区，用 LBA28
    if (lba < 0x0FFFFFFFULL && sector_count <= 256 && sector_count > 0) {
        result = ATA_Read_LBA28_cmd(channel, is_master, lba, sector_count);
    } else {
        result = ATA_Read_LBA48_cmd(channel, is_master, lba, sector_count);
    }
    if (result == 0) {
        return Read_Sector(channel, is_master, lba, sector_count, buffer);
    } else {
        return result;
    }
}

//LBA28 PIO 写入命令
int ATA_Write_LBA28_cmd(ata_channel_t *channel, _Bool is_master, uint32_t lba, uint16_t sector_count) {
    if (sector_count == 0) sector_count = 256;
    if (lba > 0x0FFFFFFF) return -4;//LBA2 超范围
    uint8_t drive_byte = 0xE0 | (is_master ? 0 : (1 << 4)) | ((lba >> 24) & 0x0F);
    outb(channel->drive_port, drive_byte);
    delay_ms(1);
    if (ATA_Wait_Busy(channel, 5000) != 0) return -2;
    outb(channel->sector_count, (uint8_t)(sector_count & 0xFF));
    outb(channel->lba_lo,       (uint8_t)(lba & 0xFF));
    outb(channel->lba_mid,      (uint8_t)((lba >> 8) & 0xFF));
    outb(channel->lba_hi,       (uint8_t)((lba >> 16) & 0xFF));
    outb(channel->drive_port,   drive_byte);//重新写一次
    outb(channel->command_port, ATA_CMD_WRITE_SECTORS);//0x30
    return 0;
}

// LBA48 PIO 写入命令
int ATA_Write_LBA48_cmd(ata_channel_t *channel, _Bool is_master, uint64_t lba, uint16_t sector_count) {
    if (sector_count == 0) sector_count = (uint16_t)65536;
    if (lba > 0x0000FFFFFFFFFFFFULL) return -4;
    uint8_t drive_byte = 0x40 | (is_master ? 0 : (1 << 4));
    outb(channel->drive_port, drive_byte);
    delay_ms(1);
    if (ATA_Wait_Busy(channel, 5000) != 0) return -2;
    //写高24位
    outb(channel->sector_count, (uint8_t)((sector_count >> 8) & 0xFF));
    outb(channel->lba_lo,       (uint8_t)((lba >> 24) & 0xFF));
    outb(channel->lba_mid,      (uint8_t)((lba >> 32) & 0xFF));
    outb(channel->lba_hi,       (uint8_t)((lba >> 40) & 0xFF));
    //写低24位
    outb(channel->sector_count, (uint8_t)(sector_count & 0xFF));
    outb(channel->lba_lo,       (uint8_t)(lba & 0xFF));
    outb(channel->lba_mid,      (uint8_t)((lba >> 8) & 0xFF));
    outb(channel->lba_hi,       (uint8_t)((lba >> 16) & 0xFF));
    outb(channel->command_port, ATA_CMD_WRITE_SECTORS_EXT);//0x34
    return 0;
}

int Write_Sector(ata_channel_t *channel, _Bool is_master, uint64_t lba,uint16_t sector_count, const void *buffer) {
    uint16_t sectors_done = 0;
    const uint16_t *word_buf = (const uint16_t *)buffer;
    while (sectors_done < sector_count) {
        //等待 BSY=0 且 DRQ=1（设备准备好接收数据）
        int drq_ret = ATA_Wait_DRP(channel, 30000);
        if (drq_ret < 0) return -(20 + sectors_done);
        //写 256 个 word（512 字节）= 一个扇区
        for (int i = 0; i < 256; i++) {
            outw(channel->data_port, word_buf[sectors_done * 256 + i]);
        }
        if (ATA_Wait_Busy(channel, 30000) != 0) return -(5 + sectors_done);//等待设备确认写完这个扇区（BSY=0）
        if (inb(channel->alt_status_port) & ATA_SR_ERR) return -(10 + sectors_done);//检查错误
        sectors_done++;
    }
    return sectors_done;
}

//自动选择LBA28/LBA48写入
int ATA_Write(ata_channel_t *channel, _Bool is_master, uint64_t lba,uint16_t sector_count, const void *buffer) {
    int result;
    //自动选择LBA28或LBA48
    if (lba < 0x0FFFFFFFULL && sector_count <= 256 && sector_count > 0) {
        result = ATA_Write_LBA28_cmd(channel, is_master, (uint32_t)lba, sector_count);
    } else {
        result = ATA_Write_LBA48_cmd(channel, is_master, lba, sector_count);
    }
    if (result != 0) return result;
    return Write_Sector(channel, is_master, lba, sector_count, buffer);//写入数据
}

//刷新ATA写缓存
void ATA_Flush_Cache(ata_channel_t *channel, _Bool is_master) {
    uint8_t drive_byte = 0xE0 | (is_master ? 0 : (1 << 4));
    outb(channel->drive_port, drive_byte);
    delay_ms(1);
    if (ATA_Wait_Busy(channel, 5000) != 0) return;
    outb(channel->command_port, ATA_CMD_FLUSH_CACHE);  // 0xE7
    ATA_Wait_Busy(channel, 30000);//等待flush完成
}

ata_disk_t FindFirstATADisk(){
    ata_disk_t result;
    memset(&result,0,sizeof(ata_disk_t));
    //Primary Master
    if(gATAInfo.drives[0].present){
        result.is_primary = true;
        result.is_master = true;
        ata_channel_t channel;
        ATA_Init_Channel(&channel,true);
        result.channel = channel;
        return result;
    }
    //Primary Slave
    if(gATAInfo.drives[1].present){
        result.is_primary = true;
        result.is_master = false;
        ata_channel_t channel;
        ATA_Init_Channel(&channel,true);
        result.channel = channel;
        return result;
    }
    //Secondary Master
    if(gATAInfo.drives[2].present){
        result.is_primary = false;
        result.is_master = true;
        ata_channel_t channel;
        ATA_Init_Channel(&channel,false);
        result.channel = channel;
        return result;
    }
    //Secondary Slava
    if(gATAInfo.drives[3].present){
        result.is_primary = false;
        result.is_master = false;
        ata_channel_t channel;
        ATA_Init_Channel(&channel,false);
        result.channel = channel;
        return result;
    }
    return result;
}
