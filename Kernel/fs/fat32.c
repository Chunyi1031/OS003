#include <fs/fat32.h>
#include <Print.h>
#include <Memory.h>
#include <Time.h>

uint16_t read_le16(const uint8_t* buf, size_t offset) {
    return (uint16_t)((uint16_t)buf[offset] | ((uint16_t)buf[offset + 1] << 8));
}
uint32_t read_le32(const uint8_t* buf, size_t offset) {
    return (uint32_t)((uint32_t)buf[offset] | ((uint32_t)buf[offset + 1] << 8) |
                      ((uint32_t)buf[offset + 2] << 16) | ((uint32_t)buf[offset + 3] << 24));
}
void write_le16(uint8_t* buf, size_t offset, uint16_t value) {
    buf[offset]     = (uint8_t)(value & 0xFF);
    buf[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
}
void write_le32(uint8_t* buf, size_t offset, uint32_t value) {
    buf[offset]     = (uint8_t)(value & 0xFF);
    buf[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
    buf[offset + 2] = (uint8_t)((value >> 16) & 0xFF);
    buf[offset + 3] = (uint8_t)((value >> 24) & 0xFF);
}

//检查dbr
_Bool check_DBR(const uint8_t* buffer) {
    if (buffer[0x1FE] != 0x55 || buffer[0x1FF] != 0xAA)return false;//检查引导签名
    //每扇区字节数
    uint16_t bytes_per_sector = read_le16(buffer, 11);
    if (bytes_per_sector != (uint16_t)512)return false;
    //每簇扇区数（必须是2的幂）
    uint8_t sectors_per_cluster = buffer[13];
    if (sectors_per_cluster == 0 || (sectors_per_cluster & (sectors_per_cluster - 1)) != 0)return false;
    if ((uint32_t)sectors_per_cluster * bytes_per_sector > 32 * 1024)return false;//每簇大小不超过32KB
    //保留扇区数
    uint16_t reserved_sectors = read_le16(buffer, 14);
    if (reserved_sectors <= 1)return false;
    //FAT表大小
    uint32_t fat_size = read_le32(buffer, 36);
    if (fat_size == 0)return false;
    return true;
}

STATUS FAT32_GetInfo(ata_disk_t *Disk, gpt_partition_entry_t *Partition, fat32_info_t *Info) {
    uint8_t buffer[512];
    uint64_t partition_lba = Partition->starting_lba;//分区起始LBA
    //读DBR
    if (ATA_Read(&Disk->channel, &Disk->is_master, partition_lba, 1, buffer) < 0)return 1;
    if (!check_DBR(buffer))return 1;
    //解析BPB基本字段
    Info->bytes_per_sector    = read_le16(buffer, 11);
    Info->sectors_per_cluster = buffer[13];
    Info->reserved_sectors    = read_le16(buffer, 14);
    Info->num_fats            = buffer[16];
    Info->sectors_per_fat     = read_le32(buffer, 36);
    Info->root_cluster        = read_le32(buffer, 44);
    Info->fsinfo_sector       = read_le16(buffer, 48);
    Info->backup_boot_sector  = read_le16(buffer, 50);
    Info->total_sectors       = read_le32(buffer, 32);
    //计算区域起始地址
    Info->fat_region_lba  = Info->reserved_sectors;//FAT表区域起始（相对分区起始的扇区偏移）
    Info->data_region_lba = Info->reserved_sectors + (Info->num_fats * Info->sectors_per_fat);//数据区起始（FAT 表区域 + 所有 FAT 表的扇区数）
    //总簇数
    uint32_t data_sectors = Info->total_sectors - Info->data_region_lba;
    Info->total_clusters  = data_sectors / Info->sectors_per_cluster;
    //解析扩展 BPB 信息
    memcpy(Info->volume_label, buffer + 71, 11);
    Info->volume_label[11] = '\0';
    //去掉尾部空格
    for (int i = 10; i >= 0; i--) {
        if (Info->volume_label[i] == ' ') {
            Info->volume_label[i] = '\0';
        } else if (Info->volume_label[i] != '\0') {
            break;
        }
    }
    Info->volume_id = read_le32(buffer, 67);//卷序列号在 offset 67，4 字节
    //读 FSInfo 扇区并解析空闲簇信息
    Info->free_clusters     = 0xFFFFFFFF;//默认为未知
    Info->next_free_cluster = 0xFFFFFFFF;
    if (ATA_Read(&Disk->channel, &Disk->is_master, partition_lba + Info->fsinfo_sector, 1, buffer) >= 0) {
        // 验证 FSInfo 签名
        if (memcmp(buffer, "RRaA", 4) == 0 && memcmp(buffer + 0x1E4, "rrAa", 4) == 0) {
            Info->free_clusters     = read_le32(buffer, 0x1E8);//空闲簇数
            Info->next_free_cluster = read_le32(buffer, 0x1EC);//下一个空闲簇提示
        }
    }
    return 0;
}

void name_to_83(const char *name, uint8_t *out_83) {
    int name_len = 0;
    int ext_len  = 0;
    char base[9] = {0};
    char ext[4]  = {0};
    memset(out_83, ' ', 11);
    const char *dot = NULL;
    int i, total = 0;
    for (i = 0; name[i]; i++) {
        if (name[i] == '.') dot = &name[i];
        total++;
    }
    if (dot) {
        int base_len = dot - name;
        if (base_len > 8) base_len = 8;
        for (i = 0; i < base_len; i++) out_83[i] = name[i];
        const char *ext_start = dot + 1;
        for (i = 0; i < 3 && ext_start[i]; i++) out_83[8 + i] = ext_start[i];
    } else {
        for (i = 0; i < 8 && name[i]; i++) out_83[i] = name[i];
    }
    for (i = 0; i < 11; i++) {
        if (out_83[i] >= 'a' && out_83[i] <= 'z')
            out_83[i] = out_83[i] - 'a' + 'A';
    }
}

//读FAT表中簇号对应的FAT表项（4 字节）
uint64_t FAT32_ClusterToLBA(fat32_info_t *Info, uint32_t cluster) {
    return Info->data_region_lba + ((uint64_t)(cluster - 2) * Info->sectors_per_cluster);
}

STATUS FAT32_NextCluster(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition,uint32_t cluster, uint32_t *next) {
    uint8_t fat_buf[512];
    //每个簇号占 4 字节，计算它在FAT表中的偏移
    uint64_t fat_offset   = (uint64_t)cluster * 4;//字节偏移
    uint64_t fat_sector   = fat_offset / Info->bytes_per_sector;//FAT表中的扇区号
    uint32_t sector_offset = fat_offset % Info->bytes_per_sector;//扇区内的字节偏移
    //计算FAT表起始LBA地址
    uint64_t abs_lba = Partition->starting_lba + Info->fat_region_lba + fat_sector;
    if (ATA_Read(&Disk->channel, &Disk->is_master, abs_lba, 1, fat_buf) < 0)return 1;
    *next = read_le32(fat_buf, sector_offset) & 0x0FFFFFFF;//高4位保留
    return 0;
}

STATUS FAT32_ReadCluster(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t cluster, void *buffer) {
    uint64_t start_lba = Partition->starting_lba + FAT32_ClusterToLBA(Info, cluster);
    uint16_t sectors   = Info->sectors_per_cluster;
    return (ATA_Read(&Disk->channel, Disk->is_master, start_lba, sectors, buffer) < 0) ? 1 : 0;
}

STATUS FAT32_WriteCluster(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t cluster, void *buffer) {
    uint64_t start_lba = Partition->starting_lba + FAT32_ClusterToLBA(Info, cluster);
    uint16_t sectors   = Info->sectors_per_cluster;
    int a = ATA_Write(&Disk->channel,Disk->is_master,start_lba,sectors,buffer);
    if(a < 0)return 1;
    ATA_Flush_Cache(&Disk->channel,Disk->is_master);
    return 0;
}

//目录项查找
//检查一个目录项是否为有效的短文件名项（不是已删除/卷标/长文件名）
static _Bool is_valid_sfn_entry(fat32_dir_entry_t *entry) {
    if (entry->name[0] == 0x00 || entry->name[0] == 0xE5 || entry->name[0] == 0x2E)return false;//0xE5 = 已删除, 0x00 = 目录结束, 0x2E = "." 或 ".."
    if (entry->attr == FAT32_ATTR_LFN)return false;//长文件名项
    if (entry->attr & FAT32_ATTR_VOLUME_ID)return false;//卷标签
    return true;
}
//8.3短文件名
STATUS FAT32_FindEntry(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition,uint32_t dir_cluster, const char *filename,fat32_dir_entry_t *out_entry) {
    //确定目录单簇大小
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint8_t *cluster_buf = NULL;
    uint32_t current_cluster = dir_cluster;
    uint32_t entry_count = 0;
    //遍历簇链
    while (1) {
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current_cluster, buf) != 0)return 2;//读取整个簇
        //遍历这个簇里的所有目录项（每个 32 字节）
        entry_count = cluster_size / sizeof(fat32_dir_entry_t);
        for (uint32_t i = 0; i < entry_count; i++) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t *)(buf + i * sizeof(fat32_dir_entry_t));
            //0x00=目录结束标志
            if (entry->name[0] == 0x00)goto next_cluster;//跳到下一个簇
            if (is_valid_sfn_entry(entry)) {
                // 比较 11 字节短文件名
                if (memcmp(entry->name, filename, 11) == 0) {
                    *out_entry = *entry;
                    return 0;//找到！
                }
            }
        }
next_cluster:
        if (FAT32_NextCluster(Info, Disk, Partition, current_cluster, &current_cluster) != 0)return 3;//获取下一个簇
        if (current_cluster >= FAT32_ENTRY_EOF)break;//检查是否链结束
    }
    return 1;//未找到
}

//读取文件
uint32_t FAT32_ReadFile(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition,fat32_dir_entry_t *entry, void *buffer) {
    if (entry->attr & FAT32_ATTR_DIRECTORY)return 0;//不能读目录
    uint32_t file_size = entry->file_size;
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t cluster = ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
    uint32_t bytes_read = 0;
    uint8_t *buf = (uint8_t *)buffer;
    //跟随簇链读取
    while (bytes_read < file_size) {
        //读当前簇
        if (FAT32_ReadCluster(Info, Disk, Partition, cluster, buf + bytes_read) != 0)return bytes_read;//返回已读的部分
        bytes_read += cluster_size;
        if (bytes_read >= file_size)break;//如果已经读了足够字节，可以提前退出
        if (FAT32_NextCluster(Info, Disk, Partition, cluster, &cluster) != 0)return bytes_read;//获取下一个簇
        if (cluster >= FAT32_ENTRY_EOF)break;//簇链结束
    }
    return (bytes_read < file_size) ? bytes_read : file_size;
}

//长文件名（LFN）支持
uint8_t fat32_lfn_checksum(const uint8_t *sfn_name) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = ((sum >> 1) | (sum << 7)) + sfn_name[i];
    }
    return sum;
}

//从LFN项的name分片里拷贝UTF-16LE字符到ASCII缓冲区
int lfn_copy_name_segment(char *out, int out_max, const void *name_part, int part_len) {
    int written = 0;
    uint16_t buf[6];
    memcpy(buf, name_part, part_len * 2);
    for (int i = 0; i < part_len && written < out_max - 1; i++) {
        uint16_t ch = buf[i];
        if (ch == 0x0000) break;//字符串结束
        if (ch == 0xFFFF) continue;//填充字符，跳过
        out[written++] = (char)(ch & 0xFF);
    }
    return written;
}

#define LFN_MAX_PIECES 20//缓冲中收集到的LFN条目

//从缓冲的LFN条目集合中重建长文件名
static void rebuild_lfn_name(fat32_lfn_entry_t *entries[], int count, char *out, int out_max) {
    int pos = 0;
    memset(out, 0, out_max);
    for (int i = count - 1; i >= 0; i--) {
        fat32_lfn_entry_t *lfn = entries[i];
        pos += lfn_copy_name_segment(out + pos, out_max - pos, lfn->name1, 5);
        pos += lfn_copy_name_segment(out + pos, out_max - pos, lfn->name2, 6);
        pos += lfn_copy_name_segment(out + pos, out_max - pos, lfn->name3, 2);
    }
    out[out_max - 1] = '\0';
}

//按文件名查找（支持长文件名）
STATUS FAT32_FindEntryAny(fat32_info_t *Info, ata_disk_t *Disk,  gpt_partition_entry_t *Partition,  uint32_t dir_cluster, const char *long_name, fat32_dir_entry_t *out_entry) {
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t current_cluster = dir_cluster;
    //把long_name转成8.3格式
    uint8_t sfn_name[11];
    name_to_83(long_name, sfn_name);
    //LFN条目缓冲区
    fat32_lfn_entry_t *lfn_pieces[LFN_MAX_PIECES];
    int lfn_count = 0;
    //遍历目录簇链
    while (1) {
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current_cluster, buf) != 0)return 2;//读取簇
        uint32_t entry_count = cluster_size / sizeof(fat32_dir_entry_t);
        for (uint32_t i = 0; i < entry_count; i++) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t *)(buf + i * 32);
            if (entry->name[0] == 0x00)goto next_cluster;//0x00 = 目录结束
            //已删除项
            if (entry->name[0] == 0xE5) {
                lfn_count = 0;
                continue;
            }
            //LFN条目（attr == 0x0F）
            if (entry->attr == FAT32_ATTR_LFN) {
                if (lfn_count < LFN_MAX_PIECES) {
                    lfn_pieces[lfn_count] = (fat32_lfn_entry_t *)entry;
                    lfn_count++;
                }
                continue;
            }
            //检查 LFN 完整性
            _Bool lfn_valid = false;
            if (lfn_count > 0) {
                //验证校验和
                uint8_t expected_cs = fat32_lfn_checksum(entry->name);
                if (lfn_pieces[lfn_count - 1]->checksum == expected_cs)
                    lfn_valid = true;
            }
            //跳过"."和".."
            if (entry->name[0] == 0x2E) {
                lfn_count = 0;
                continue;
            }
            //跳过卷标签
            if (entry->attr & FAT32_ATTR_VOLUME_ID) {
                lfn_count = 0;
                continue;
            }
            //尝试匹配长文件名
            _Bool matched = false;
            if (lfn_valid && lfn_count > 0) {
                //从LFN条目重建长文件名
                char lfn_name[260];
                rebuild_lfn_name(lfn_pieces, lfn_count, lfn_name, sizeof(lfn_name));
                //比较
                if (strcmp(lfn_name, (char*)long_name) == 0) {
                    matched = true;
                }
            }
            //尝试匹配短文件名
            if (!matched) {
                if (memcmp(entry->name, sfn_name, 11) == 0) {
                    matched = true;
                }
            }
            //清空 LFN 缓冲，为下一个条目做准备
            lfn_count = 0;
            if (matched) {
                *out_entry = *entry;
                return 0;
            }
        }
next_cluster:
        //获取下一个簇
        if (FAT32_NextCluster(Info, Disk, Partition, current_cluster, &current_cluster) != 0)return 3;
        if (current_cluster >= FAT32_ENTRY_EOF)break;
    }
    return 1;  // 未找到
}

fat32_list_entry_t* FAT32_ListDirectory(fat32_info_t *Info,ata_disk_t *Disk,gpt_partition_entry_t *Partition,uint32_t dir_cluster,int *count,int *out_pages){
    //计算数据
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t current_cluster = dir_cluster;
    int max_per_cluster = cluster_size / 32;
    int capacity = max_per_cluster * 8;//最多8个簇
    int alloc_pages = (capacity * sizeof(fat32_list_entry_t) + 4095) / 4096;
    fat32_list_entry_t *result = Pmm_Malloc(alloc_pages);
    if(out_pages) *out_pages = alloc_pages;
    int real_count = 0;
    //LFN条目缓冲
    #define LFN_BUF_MAX 20
    fat32_lfn_entry_t *lfn_buf[LFN_BUF_MAX];
    int lfn_count = 0;
    while (1) {
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current_cluster, buf) != 0)return NULL;
        int entry_count = cluster_size / 32;
        for (int i = 0; i < entry_count; i++) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t *)(buf + i * 32);
            if (entry->name[0] == 0x00)goto next_cluster;//目录结束
            //已删除
            if (entry->name[0] == 0xE5) {
                lfn_count = 0;
                continue;
            }
            //"."和".."
            if (entry->name[0] == 0x2E) {
                lfn_count = 0;
                continue;
            }
            //长文件名条目（attr == 0x0F）
            if (entry->attr == FAT32_ATTR_LFN) {
                if (lfn_count < LFN_BUF_MAX) {
                    lfn_buf[lfn_count] = (fat32_lfn_entry_t *)entry;
                    lfn_count++;
                }
                continue;
            }
            //卷标
            if (entry->attr & FAT32_ATTR_VOLUME_ID) {
                lfn_count = 0;
                continue;
            }
            //检查LFN有效性
            _Bool use_lfn = false;
            if (lfn_count > 0) {
                uint8_t expected_cs = fat32_lfn_checksum(entry->name);
                if (lfn_buf[lfn_count - 1]->checksum == expected_cs)use_lfn = true;
            }
            if (real_count >= capacity)goto done;//存结果
            result[real_count].entry = *entry;
            if (use_lfn) {
                //从LFN缓冲重建长文件名
                fat32_lfn_entry_t *ordered[LFN_BUF_MAX];
                int pos = 0;
                for (int j = lfn_count - 1; j >= 0; j--)ordered[pos++] = lfn_buf[j];
                rebuild_lfn_name(ordered, lfn_count, result[real_count].display_name, FAT32_MAX_NAME);
            } else {
                //用 8.3 短文件名，去掉尾部空格
                memcpy(result[real_count].display_name, entry->name, 11);
                result[real_count].display_name[11] = '\0';
                //去掉尾部空格
                for (int j = 10; j >= 0; j--) {
                    if (result[real_count].display_name[j] == ' ')result[real_count].display_name[j] = '\0';
                    else break;
                }
            }
            real_count++;
            lfn_count = 0;  // 清空 LFN 缓冲
        }
next_cluster:
        if (FAT32_NextCluster(Info, Disk, Partition, current_cluster, &current_cluster) != 0)return NULL;
        if (current_cluster >= FAT32_ENTRY_EOF)break;
    }
done:
    *count = real_count;
    return result;
}

//FAT表项写入
STATUS FAT32_WriteFATEntry(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition, uint32_t cluster, uint32_t value){
    //计算数据
    uint8_t fat_buf[512];
    uint64_t fat_offset = (uint64_t)cluster * 4;
    uint64_t fat_sector = fat_offset / Info->bytes_per_sector;
    uint32_t sector_off = fat_offset % Info->bytes_per_sector;
    //同步更新所有FAT表
    for (uint8_t i = 0; i < Info->num_fats; i++) {
        uint64_t abs_lba = Partition->starting_lba + Info->fat_region_lba + (uint64_t)i * Info->sectors_per_fat + fat_sector;//计算物理LBA地址
        if (ATA_Read(&Disk->channel, &Disk->is_master, abs_lba, 1, fat_buf) < 0)return 1;//读取FAT表
        write_le32(fat_buf, sector_off, value);//修改FAT表
        if (ATA_Write(&Disk->channel, &Disk->is_master, abs_lba, 1, fat_buf) < 0)return 1;//将新FAT表写入硬盘
    }
    ATA_Flush_Cache(&Disk->channel, &Disk->is_master);//刷新
    return 0;
}

//分配一个空闲簇
uint32_t FAT32_AllocCluster(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition){
    //计算簇号
    uint32_t start = (Info->next_free_cluster != 0xFFFFFFFF &&Info->next_free_cluster >= 2) ? Info->next_free_cluster : 2;
    uint32_t cluster = start;
    uint8_t fat_buf[512];
    do {
        uint64_t offset = (uint64_t)cluster * 4;
        uint64_t sec    = offset / Info->bytes_per_sector;
        uint32_t off    = offset % Info->bytes_per_sector;
        uint64_t lba    = Partition->starting_lba + Info->fat_region_lba + sec;
        //读取空闲簇
        if (ATA_Read(&Disk->channel, &Disk->is_master, lba, 1, fat_buf) < 0)return 0;
        if ((read_le32(fat_buf, off) & 0x0FFFFFFF) == FAT32_ENTRY_FREE) {
            if (FAT32_WriteFATEntry(Info, Disk, Partition, cluster, FAT32_ENTRY_EOF) != 0)return 0;//标记为链尾
            //更新FSInfo的空闲计数
            if ((Info->free_clusters != 0xFFFFFFFF) && (Info->free_clusters > 0))Info->free_clusters--;
            Info->next_free_cluster = cluster + 1;
            return cluster;//返回簇号
        }
        cluster++;
        if (cluster >= Info->total_clusters + 2) cluster = 2;
    } while (cluster != start);
    return 0;//磁盘已满
}

//释放一条簇链
STATUS FAT32_FreeClusterChain(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition, uint32_t start_cluster){
    uint32_t cluster = start_cluster;
    uint32_t next;
    while (cluster >= 2 && cluster < FAT32_ENTRY_EOF) {
        if (FAT32_NextCluster(Info, Disk, Partition, cluster, &next) != 0)return 1;
        if (FAT32_WriteFATEntry(Info, Disk, Partition, cluster, FAT32_ENTRY_FREE) != 0)return 1;
        if (Info->free_clusters != 0xFFFFFFFF) Info->free_clusters++;
        if (next >= FAT32_ENTRY_EOF) break;
        cluster = next;
    }
    return 0;
}

//在磁盘上找到目录项并直接覆盖
STATUS FAT32_WriteDirEntry(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition, uint32_t dir_cluster,fat32_dir_entry_t *new_entry){
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t current = dir_cluster;
    while (1) {
        //读取目录项
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current, buf) != 0)return 2;
        uint32_t n = cluster_size / sizeof(fat32_dir_entry_t);
        for (uint32_t i = 0; i < n; i++) {
            fat32_dir_entry_t *e = (fat32_dir_entry_t *)(buf + i * 32);
            //通过短文件名匹配目录项
            if (e->name[0] == 0x00) goto wdc_next;
            if (!is_valid_sfn_entry(e)) continue;
            if (memcmp(e->name, new_entry->name, 11) == 0) {
                memcpy(e, new_entry, sizeof(fat32_dir_entry_t));//覆盖目录项
                return FAT32_WriteCluster(Info, Disk, Partition, current, buf);//写入硬盘
            }
        }
wdc_next:
        //继续下一个簇
        if (FAT32_NextCluster(Info, Disk, Partition, current, &current) != 0)return 3;
        if (current >= FAT32_ENTRY_EOF) break;
    }
    return 1;//未找到
}

//文件写入（覆盖模式）

uint32_t FAT32_WriteFile(fat32_info_t *Info, ata_disk_t *Disk,gpt_partition_entry_t *Partition, uint32_t dir_cluster,fat32_dir_entry_t *entry, const void *data, uint32_t size){
    if (entry->attr & FAT32_ATTR_DIRECTORY) return 0;//不能写目录
    //计算数据
    uint32_t cluster_size   = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t start_cluster  = ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
    uint32_t old_clusters   = (entry->file_size + cluster_size - 1) / cluster_size;
    uint8_t *src            = (uint8_t *)data;
    uint32_t written        = 0;
    uint32_t cluster        = start_cluster;
    //遇到空文件，分配簇
    if (start_cluster == 0 && size > 0) {
        cluster = FAT32_AllocCluster(Info, Disk, Partition);
        if (cluster == 0) return 0;
        start_cluster = cluster;
        entry->cluster_low  = cluster & 0xFFFF;
        entry->cluster_high = (cluster >> 16) & 0xFFFF;
    }
    //逐簇写入
    while (written < size) {
        uint32_t remain = size - written;
        uint32_t to_write = (remain < cluster_size) ? remain : cluster_size;
        //最后一簇不满，不破坏簇内原有数据
        if (to_write < cluster_size) {
            uint8_t tmp[cluster_size];//簇缓冲区
            FAT32_ReadCluster(Info, Disk, Partition, cluster, tmp);
            memcpy(tmp, src + written, to_write);
            if (FAT32_WriteCluster(Info, Disk, Partition, cluster, tmp) != 0)return written;
        } else {
            if (FAT32_WriteCluster(Info, Disk, Partition, cluster,src + written) != 0)return written;
        }
        written += to_write;
        if (written >= size) break;//如果写入量足够，退出
        //获取下一个簇
        uint32_t next;
        if (FAT32_NextCluster(Info, Disk, Partition, cluster, &next) != 0)return written;
        //如果链不够长，分配新簇并链接
        if (next >= FAT32_ENTRY_EOF) {
            next = FAT32_AllocCluster(Info, Disk, Partition);
            if (next == 0) return written;
            if (FAT32_WriteFATEntry(Info, Disk, Partition, cluster, next) != 0) return written;
        }
        cluster = next;
    }
    //如果新大小比旧大小小，释放多余簇
    if (written < old_clusters * cluster_size) {
        uint32_t after_last;
        if (FAT32_NextCluster(Info, Disk, Partition, cluster, &after_last) == 0 &&after_last < FAT32_ENTRY_EOF) {
            FAT32_WriteFATEntry(Info, Disk, Partition, cluster, FAT32_ENTRY_EOF);
            FAT32_FreeClusterChain(Info, Disk, Partition, after_last);
        }
    }
    //更新目录项
    if(written > entry->file_size)entry->file_size = written;
    FAT32_WriteDirEntry(Info, Disk, Partition, dir_cluster, entry);
    return written;
}

//文件写入（追加模式）
uint32_t FAT32_AppendFile(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, fat32_dir_entry_t *entry, const void *data, uint32_t size) {
    if (size == 0 || (entry->attr & FAT32_ATTR_DIRECTORY)) return 0;
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t old_size = entry->file_size;
    uint32_t start = ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
    uint8_t *src = (uint8_t *)data;
    uint32_t written = 0;
    uint32_t cluster;
    //如果是空文件，分配第一个簇
    if (start == 0) {
        cluster = FAT32_AllocCluster(Info, Disk, Partition);
        if (cluster == 0) return 0;
        entry->cluster_low = cluster & 0xFFFF;
        entry->cluster_high = (cluster >> 16) & 0xFFFF;
    } else {
        //走到簇链最后一个簇
        cluster = start;
        uint32_t next;
        while (1) {
            if (FAT32_NextCluster(Info, Disk, Partition, cluster, &next) != 0) return written;
            if (next >= FAT32_ENTRY_EOF) break;
            cluster = next;
        }
    }
    uint32_t offset = old_size % cluster_size;//最后一个簇中已有数据占用的字节数
    //如果最后一个簇恰好写满，分配新簇
    if (offset == 0 && old_size > 0) {
        uint32_t nc = FAT32_AllocCluster(Info, Disk, Partition);
        if (nc == 0) return written;
        FAT32_WriteFATEntry(Info, Disk, Partition, cluster, nc);
        cluster = nc;
    }
    //从offset开始写入
    while (written < size) {
        uint32_t remain = size - written;
        uint32_t space = cluster_size - offset;
        uint32_t to_write = (remain < space) ? remain : space;
        //写当前簇
        if (offset > 0 || to_write < cluster_size) {
            uint8_t tmp[cluster_size];
            FAT32_ReadCluster(Info, Disk, Partition, cluster, tmp);
            memcpy(tmp + offset, src + written, to_write);
            FAT32_WriteCluster(Info, Disk, Partition, cluster, tmp);
        } else {
            FAT32_WriteCluster(Info, Disk, Partition, cluster, src + written);
        }
        written += to_write;
        offset += to_write;
        if (written >= size) break;
        //如果当前簇写满，分配新簇
        uint32_t nc = FAT32_AllocCluster(Info, Disk, Partition);
        if (nc == 0) return written;
        FAT32_WriteFATEntry(Info, Disk, Partition, cluster, nc);
        cluster = nc;
        offset = 0;
    }
    //更新目录项
    entry->file_size = old_size + written;
    FAT32_WriteDirEntry(Info, Disk, Partition, dir_cluster, entry);
    return written;
}

//生成LFN条目到缓冲区
int make_lfn_entries(const char *name, fat32_lfn_entry_t *out, uint8_t sfn_cs) {
    int name_len = strlen(name);
    if (name_len == 0) return 0;
    int total = (name_len + 12) / 13;//每个LFN条目存13个UTF-16字符
    for (int i = 0; i < total; i++) {
        fat32_lfn_entry_t *e = &out[i];
        memset(e, 0xFF, sizeof(fat32_lfn_entry_t));//所有字节默认0xFFFF
        //头部字段
        e->order = (uint8_t)((total - i) | ((i == 0) ? 0x40 : 0x00));
        e->attr = FAT32_ATTR_LFN;
        e->type = 0;
        e->checksum = sfn_cs;
        e->first_cluster = 0;
        int start = i * 13;
        int remain = name_len - start;
        if (remain <= 0) { e->name1[0] = 0x0000; continue; }
        //用临时缓冲区整理13个字符
        uint16_t buf[13];
        for (int j = 0; j < 13; j++) buf[j] = 0xFFFF;
        int j;
        for (j = 0; j < 13; j++) {
            int idx = start + j;
            if (idx < name_len)
                buf[j] = (uint16_t)(unsigned char)name[idx];
            else {
                buf[j] = 0x0000;//终止符
                break;
            }
        }
        //拷贝到条目对应字段
        memcpy(e->name1, buf, 10);        //5 × uint16_t
        memcpy(e->name2, buf + 5, 12);    //6 × uint16_t
        memcpy(e->name3, buf + 11, 4);    //2 × uint16_t
    }
    return total;
}

//修改文件名
STATUS FAT32_RenameFile(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, const char *old_name, const char *new_name) {
    //检查新文件名是否已存在
    fat32_dir_entry_t existing;
    if (FAT32_FindEntryAny(Info, Disk, Partition, dir_cluster, new_name, &existing) == 0)return 4;
    //预计算
    uint8_t new_sfn[11];
    name_to_83(new_name, new_sfn);
    uint8_t new_sfn_cs = fat32_lfn_checksum(new_sfn);
    uint8_t old_sfn[11];
    name_to_83(old_name, old_sfn);
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t current_cluster = dir_cluster;
    //为新名生成LFN条目（预计算条目数）
    fat32_lfn_entry_t new_lfn_buf[20];//最多20个LFN条目 = 260字符
    int new_lfn_needed = make_lfn_entries(new_name, new_lfn_buf, new_sfn_cs);
    //簇内LFN跟踪
    #define LFN_RENAME_MAX 20
    uint32_t lfn_offsets[LFN_RENAME_MAX];
    int lfn_count = 0;
    while (1) {
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current_cluster, buf) != 0)return 2;
        uint32_t entry_count = cluster_size / 32;
        lfn_count = 0;
        for (uint32_t i = 0; i < entry_count; i++) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t *)(buf + i * 32);
            if (entry->name[0] == 0x00)goto rnf_next_cluster;
            if (entry->name[0] == 0xE5) { lfn_count = 0; continue; }
            if (entry->attr == FAT32_ATTR_LFN) {
                if (lfn_count < LFN_RENAME_MAX) { lfn_offsets[lfn_count] = i * 32; lfn_count++; }
                continue;
            }
            if (entry->name[0] == 0x2E) { lfn_count = 0; continue; }
            if (entry->attr & FAT32_ATTR_VOLUME_ID) { lfn_count = 0; continue; }
            //匹配 old_name
            _Bool matched = false;
            if (lfn_count > 0) {
                uint8_t cs = fat32_lfn_checksum(entry->name);
                fat32_lfn_entry_t *last = (fat32_lfn_entry_t *)(buf + lfn_offsets[lfn_count - 1]);
                if (last->checksum == cs) {
                    fat32_lfn_entry_t *ordered[LFN_RENAME_MAX];
                    int pos = 0;
                    for (int j = lfn_count - 1; j >= 0; j--)
                        ordered[pos++] = (fat32_lfn_entry_t *)(buf + lfn_offsets[j]);
                    char lfn_name[260];
                    rebuild_lfn_name(ordered, lfn_count, lfn_name, sizeof(lfn_name));
                    if (strcmp(lfn_name, (char*)old_name) == 0) matched = true;
                }
            }
            if (!matched && memcmp(entry->name, old_sfn, 11) == 0) matched = true;
            if (!matched) { lfn_count = 0; continue; }
            //找到，执行重命名
            //如果LFN条目数相同且在同一个簇内，更新LFN
            if (lfn_count > 0 && new_lfn_needed > 0 && lfn_count == new_lfn_needed) {
                //原地更新所有LFN条目
                for (int j = 0; j < lfn_count; j++) {
                    memcpy(buf + lfn_offsets[j], &new_lfn_buf[j], sizeof(fat32_lfn_entry_t));
                }
                //更新SFN + 校验和
                memcpy(entry->name, new_sfn, 11);
            } else {
                //删除旧LFN（写0xE5）
                for (int j = 0; j < lfn_count; j++)buf[lfn_offsets[j]] = 0xE5;
                //更新SFN
                memcpy(entry->name, new_sfn, 11);
            }
            //写回簇
            if (FAT32_WriteCluster(Info, Disk, Partition, current_cluster, buf) != 0)return 3;
            ATA_Flush_Cache(&Disk->channel, &Disk->is_master);
            return 0;
        }
rnf_next_cluster:
        if (FAT32_NextCluster(Info, Disk, Partition, current_cluster, &current_cluster) != 0)return 3;
        if (current_cluster >= FAT32_ENTRY_EOF) break;
    }
    return 1;
}

//将RTC时间转为FAT格式
uint16_t fat_pack_date(uint16_t year, uint8_t month, uint8_t day) {
    return (uint16_t)(((uint16_t)(year - 1980) << 9) | ((uint16_t)month << 5) | (uint16_t)day);
}
uint16_t fat_pack_time(uint8_t hour, uint8_t minute, uint8_t second) {
    return (uint16_t)(((uint16_t)hour << 11) | ((uint16_t)minute << 5) | ((uint16_t)(second / 2)));
}

//创建空文件
STATUS FAT32_CreateFile(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition,uint32_t dir_cluster,const char *filename) {
    //检查是否已存在
    fat32_dir_entry_t tmp;
    if (FAT32_FindEntryAny(Info, Disk, Partition, dir_cluster, filename, &tmp) == 0)return 1;
    //预计算SFN + LFN
    uint8_t new_sfn[11];
    name_to_83(filename, new_sfn);
    uint8_t sfn_cs = fat32_lfn_checksum(new_sfn);
    fat32_lfn_entry_t lfn_buf[20];
    int lfn_needed = make_lfn_entries(filename, lfn_buf, sfn_cs);
    int needed = lfn_needed + 1;//LFN条目+SFN条目
    //获取当前时间
    struct rtc_time rtc;
    get_rtc_time(&rtc);
    uint16_t fatdate = fat_pack_date(rtc.year + 2000, rtc.month, rtc.day);
    uint16_t fattime = fat_pack_time(rtc.hour, rtc.minute, rtc.second);
    //遍历目录找空位
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t current = dir_cluster;
    uint32_t prev_cluster = 0;
    _Bool first_cluster = (dir_cluster == current) ? true : false;
    while (1) {
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current, buf) != 0) return 2;
        //在当前簇中扫描空闲槽位
        int free_run = 0;//当前连续空闲条目数
        int free_start = -1;//空闲段起始条目索引
        uint32_t entry_count = cluster_size / 32;
        for (uint32_t i = 0; i < entry_count; i++) {
            fat32_dir_entry_t *e = (fat32_dir_entry_t *)(buf + i * 32);
            if (e->name[0] == 0x00) {
                //0x00 = 从此到簇尾全部可用
                if (free_run == 0) free_start = i;
                if ((int)(entry_count - i) >= needed)goto ccf_write;//如果空间够，写在这里
                goto ccf_next;//如果不够，当前簇空间不足，跳到下一个簇
            } else if (e->name[0] == 0xE5) {
                //已删除条目，可以重用
                if (free_run == 0) free_start = i;
                free_run++;
                if (free_run >= needed) goto ccf_write;//够了
            } else {
                free_run = 0;//被占用条目打断了连续空位
            }
        }
ccf_next:
        //当前簇空间不足，继续下一个簇
        if (FAT32_NextCluster(Info, Disk, Partition, current, &current) != 0) return 2;
        if (current >= FAT32_ENTRY_EOF) {
            //簇链已结束，需要分配新簇给目录
            uint32_t new_cluster = FAT32_AllocCluster(Info, Disk, Partition);
            if (new_cluster == 0) return 5;
            if (FAT32_WriteFATEntry(Info, Disk, Partition, current == dir_cluster ? dir_cluster : current, new_cluster) != 0) return 3;//把新簇链接到链尾
            //新簇清零
            uint8_t zero[cluster_size];
            memset(zero, 0, cluster_size);
            if (FAT32_WriteCluster(Info, Disk, Partition, new_cluster, zero) != 0) return 3;
            current = new_cluster;
            //在新簇开头写入
            free_start = 0;
            goto ccf_write;
        }
        //否则继续循环扫描下一个簇
        continue;
ccf_write:;
        //在free_start位置写入
        int pos = free_start;
        //先写LFN条目
        for (int j = 0; j < lfn_needed; j++) {
            memcpy(buf + pos * 32, &lfn_buf[j], sizeof(fat32_lfn_entry_t));
            pos++;
        }
        //再写SFN条目
        fat32_dir_entry_t *sfn = (fat32_dir_entry_t *)(buf + pos * 32);
        memset(sfn, 0, sizeof(fat32_dir_entry_t));
        memcpy(sfn->name, new_sfn, 11);
        sfn->attr = FAT32_ATTR_ARCHIVE;//普通文件
        sfn->creation_time = fattime;
        sfn->creation_date = fatdate;
        sfn->last_access_date = fatdate;
        sfn->last_write_time = fattime;
        sfn->last_write_date = fatdate;
        sfn->cluster_high = 0;
        sfn->cluster_low = 0;
        sfn->file_size = 0;
        //写回簇
        if (FAT32_WriteCluster(Info, Disk, Partition, current, buf) != 0) return 3;
        ATA_Flush_Cache(&Disk->channel, &Disk->is_master);
        return 0;
    }
}

STATUS FAT32_CreateDir(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, const char *dirname){
    //检查是否已存在
    fat32_dir_entry_t tmp;
    if (FAT32_FindEntryAny(Info, Disk, Partition, dir_cluster, dirname, &tmp) == 0)return 1;
    //预计算SFN + LFN
    uint8_t new_sfn[11];
    name_to_83(dirname, new_sfn);
    uint8_t sfn_cs = fat32_lfn_checksum(new_sfn);
    fat32_lfn_entry_t lfn_buf[20];
    int lfn_needed = make_lfn_entries(dirname, lfn_buf, sfn_cs);
    int needed = lfn_needed + 1;//LFN条目+SFN条目
    //获取当前时间
    struct rtc_time rtc;
    get_rtc_time(&rtc);
    uint16_t fatdate = fat_pack_date(rtc.year + 2000, rtc.month, rtc.day);
    uint16_t fattime = fat_pack_time(rtc.hour, rtc.minute, rtc.second);
    //分配一个数据簇给新目录
    uint32_t new_cluster = FAT32_AllocCluster(Info, Disk, Partition);
    if (new_cluster == 0) return 5;
    //初始化新簇
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint8_t dir_data[cluster_size];
    memset(dir_data, 0, cluster_size);
    //.条目
    fat32_dir_entry_t *dot = (fat32_dir_entry_t *)dir_data;
    memset(dot->name, 0x20, 11);
    dot->name[0] = 0x2E;
    dot->attr = FAT32_ATTR_DIRECTORY;
    dot->creation_time = fattime;
    dot->creation_date = fatdate;
    dot->last_access_date = fatdate;
    dot->last_write_time = fattime;
    dot->last_write_date = fatdate;
    dot->cluster_low  = (uint16_t)(new_cluster & 0xFFFF);
    dot->cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);
    dot->file_size = 0;
    //..条目
    fat32_dir_entry_t *dotdot = (fat32_dir_entry_t *)(dir_data + 32);
    memset(dotdot->name, 0x20, 11);
    dotdot->name[0] = 0x2E;
    dotdot->name[1] = 0x2E;
    dotdot->attr = FAT32_ATTR_DIRECTORY;
    dotdot->creation_time = fattime;
    dotdot->creation_date = fatdate;
    dotdot->last_access_date = fatdate;
    dotdot->last_write_time = fattime;
    dotdot->last_write_date = fatdate;
    //父目录是根目录时，.. 的簇号为0
    uint32_t parent_cluster = (dir_cluster == Info->root_cluster) ? 0 : dir_cluster;
    dotdot->cluster_low  = (uint16_t)(parent_cluster & 0xFFFF);
    dotdot->cluster_high = (uint16_t)((parent_cluster >> 16) & 0xFFFF);
    dotdot->file_size = 0;
    //写回新簇
    if (FAT32_WriteCluster(Info, Disk, Partition, new_cluster, dir_data) != 0) return 3;
    uint32_t current = dir_cluster;//在父目录中创建条目
    while (1) {
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current, buf) != 0) return 2;
        //在当前簇中扫描空闲槽位
        int free_run = 0;//当前连续空闲条目数
        int free_start = -1;//空闲段起始条目索引
        uint32_t entry_count = cluster_size / 32;
        for (uint32_t i = 0; i < entry_count; i++) {
            fat32_dir_entry_t *e = (fat32_dir_entry_t *)(buf + i * 32);
            if (e->name[0] == 0x00) {
                //0x00 = 从此到簇尾全部可用
                if (free_run == 0) free_start = i;
                if ((int)(entry_count - i) >= needed)goto cdir_write;//如果空间够，写在这里
                goto cdir_next;//如果不够，当前簇空间不足，跳到下一个簇
            } else if (e->name[0] == 0xE5) {
                //已删除条目，可以重用
                if (free_run == 0) free_start = i;
                free_run++;
                if (free_run >= needed) goto cdir_write;//够了
            } else {
                free_run = 0;//被占用条目打断了连续空位
            }
        }
cdir_next:
        //当前簇空间不足，继续下一个簇
        if (FAT32_NextCluster(Info, Disk, Partition, current, &current) != 0) return 2;
        if (current >= FAT32_ENTRY_EOF) {
            //分配新簇给父目录
            uint32_t alloc = FAT32_AllocCluster(Info, Disk, Partition);
            if (alloc == 0) return 4;
            if (FAT32_WriteFATEntry(Info, Disk, Partition,
                    current == dir_cluster ? dir_cluster : current, alloc) != 0) return 3;
            uint8_t zero[cluster_size];
            memset(zero, 0, cluster_size);
            if (FAT32_WriteCluster(Info, Disk, Partition, alloc, zero) != 0) return 3;
            current = alloc;
            free_start = 0;
            goto cdir_write;
        }
        continue;//否则继续循环扫描下一个簇
cdir_write:;
        int pos = free_start;//在free_start位置写入
        //先写LFN条目
        for (int j = 0; j < lfn_needed; j++) {
            memcpy(buf + pos * 32, &lfn_buf[j], sizeof(fat32_lfn_entry_t));
            pos++;
        }
        //再写SFN条目
        fat32_dir_entry_t *sfn = (fat32_dir_entry_t *)(buf + pos * 32);
        memset(sfn, 0, sizeof(fat32_dir_entry_t));
        memcpy(sfn->name, new_sfn, 11);
        sfn->attr = FAT32_ATTR_DIRECTORY;//目录
        sfn->creation_time = fattime;
        sfn->creation_date = fatdate;
        sfn->last_access_date = fatdate;
        sfn->last_write_time = fattime;
        sfn->last_write_date = fatdate;
        sfn->cluster_low  = (uint16_t)(new_cluster & 0xFFFF);
        sfn->cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);
        sfn->file_size = 0;
        //写回簇
        if (FAT32_WriteCluster(Info, Disk, Partition, current, buf) != 0) return 3;
        ATA_Flush_Cache(&Disk->channel, &Disk->is_master);
        return 0;
    }
}

//检查一个目录簇链是否为空
_Bool is_dir_empty(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t start_cluster) {
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    if (start_cluster == 0) return true;//完全空的目录
    uint32_t current = start_cluster;
    while (1) {
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current, buf) != 0) return false;
        uint32_t entry_count = cluster_size / 32;
        for (uint32_t i = 0; i < entry_count; i++) {
            fat32_dir_entry_t *e = (fat32_dir_entry_t *)(buf + i * 32);
            if (e->name[0] == 0x00) return true;//目录结束，为空
            if (e->name[0] == 0xE5) continue;//已删除，跳过
            if (e->attr == FAT32_ATTR_LFN) continue;//LFN条目，跳过
            if (e->name[0] == 0x2E) continue;//. 或 ..，跳过
            if (e->attr & FAT32_ATTR_VOLUME_ID) continue;//卷标，跳过
            return false;//有其他有效条目，为非空
        }
        //当前簇扫完，继续下一簇
        uint32_t next;
        if (FAT32_NextCluster(Info, Disk, Partition, current, &next) != 0) return false;
        if (next >= FAT32_ENTRY_EOF) break;
        current = next;
    }
    return true;
}

//删除空目录
STATUS FAT32_RemoveDir(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, const char *dirname) {
    //预计算短文件名
    uint8_t target_sfn[11];
    name_to_83(dirname, target_sfn);
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t current = dir_cluster;
    //跟踪LFN
    #define RD_LFN_MAX 20
    uint32_t lfn_offsets[RD_LFN_MAX];
    int lfn_count = 0;
    while (1) {
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current, buf) != 0) return 2;
        uint32_t entry_count = cluster_size / 32;
        lfn_count = 0;
        for (uint32_t i = 0; i < entry_count; i++) {
            //查找目录
            fat32_dir_entry_t *e = (fat32_dir_entry_t *)(buf + i * 32);
            if (e->name[0] == 0x00) goto rmd_next;
            if (e->name[0] == 0xE5) { lfn_count = 0; continue; }
            if (e->attr == FAT32_ATTR_LFN) {
                if (lfn_count < RD_LFN_MAX) { lfn_offsets[lfn_count] = i * 32; lfn_count++; }
                continue;
            }
            if (e->name[0] == 0x2E) { lfn_count = 0; continue; }
            if (e->attr & FAT32_ATTR_VOLUME_ID) { lfn_count = 0; continue; }
            //匹配 dirname
            _Bool matched = false;
            if (lfn_count > 0) {
                uint8_t cs = fat32_lfn_checksum(e->name);
                fat32_lfn_entry_t *last = (fat32_lfn_entry_t *)(buf + lfn_offsets[lfn_count - 1]);
                if (last->checksum == cs) {
                    fat32_lfn_entry_t *ordered[RD_LFN_MAX];
                    int pos = 0;
                    for (int j = lfn_count - 1; j >= 0; j--)ordered[pos++] = (fat32_lfn_entry_t *)(buf + lfn_offsets[j]);
                    char lfn_name[260];
                    rebuild_lfn_name(ordered, lfn_count, lfn_name, sizeof(lfn_name));
                    if (strcmp(lfn_name, (char*)dirname) == 0) matched = true;
                }
            }
            if (!matched && memcmp(e->name, target_sfn, 11) == 0) matched = true;
            if (!matched) { lfn_count = 0; continue; }
            if (!(e->attr & FAT32_ATTR_DIRECTORY)) return 4;//验证是目录
            uint32_t del_cluster = ((uint32_t)e->cluster_high << 16) | e->cluster_low;//读取被删目录的数据簇号
            //如果分配了数据簇，检查是否为空
            if (del_cluster != 0) {
                if (!is_dir_empty(Info, Disk, Partition, del_cluster))return 5;
                if (FAT32_FreeClusterChain(Info, Disk, Partition, del_cluster) != 0)return 3;//释放数据簇链
            }
            for (int j = 0; j < lfn_count; j++)buf[lfn_offsets[j]] = 0xE5;//标记父目录中的LFN条目为已删除
            e->name[0] = 0xE5;//标记SFN条目为已删除
            //写回父目录簇
            if (FAT32_WriteCluster(Info, Disk, Partition, current, buf) != 0) return 3;
            ATA_Flush_Cache(&Disk->channel, &Disk->is_master);
            return 0;
        }
rmd_next:
        if (FAT32_NextCluster(Info, Disk, Partition, current, &current) != 0) return 2;
        if (current >= FAT32_ENTRY_EOF) break;
    }
    return 1;//未找到
}

//删除文件
STATUS FAT32_DeleteFile(fat32_info_t *Info, ata_disk_t *Disk, gpt_partition_entry_t *Partition, uint32_t dir_cluster, const char *filename) {
    uint8_t target_sfn[11];
    name_to_83(filename, target_sfn);
    uint32_t cluster_size = Info->bytes_per_sector * Info->sectors_per_cluster;
    uint32_t current = dir_cluster;
    #define DF_LFN_MAX 20
    uint32_t lfn_offsets[DF_LFN_MAX];
    int lfn_count = 0;
    while (1) {
        //寻找文件目录项
        uint8_t buf[cluster_size];
        if (FAT32_ReadCluster(Info, Disk, Partition, current, buf) != 0) return 2;
        uint32_t entry_count = cluster_size / 32;
        lfn_count = 0;
        for (uint32_t i = 0; i < entry_count; i++) {
            fat32_dir_entry_t *e = (fat32_dir_entry_t *)(buf + i * 32);
            if (e->name[0] == 0x00) goto df_next;
            if (e->name[0] == 0xE5) { lfn_count = 0; continue; }
            if (e->attr == FAT32_ATTR_LFN) {
                if (lfn_count < DF_LFN_MAX) { lfn_offsets[lfn_count] = i * 32; lfn_count++; }
                continue;
            }
            if (e->name[0] == 0x2E) { lfn_count = 0; continue; }
            if (e->attr & FAT32_ATTR_VOLUME_ID) { lfn_count = 0; continue; }
            //匹配 filename
            _Bool matched = false;
            if (lfn_count > 0) {
                uint8_t cs = fat32_lfn_checksum(e->name);
                fat32_lfn_entry_t *last = (fat32_lfn_entry_t *)(buf + lfn_offsets[lfn_count - 1]);
                if (last->checksum == cs) {
                    fat32_lfn_entry_t *ordered[DF_LFN_MAX];
                    int pos = 0;
                    for (int j = lfn_count - 1; j >= 0; j--)ordered[pos++] = (fat32_lfn_entry_t *)(buf + lfn_offsets[j]);
                    char lfn_name[260];
                    rebuild_lfn_name(ordered, lfn_count, lfn_name, sizeof(lfn_name));
                    if (strcmp(lfn_name, (char*)filename) == 0) matched = true;
                }
            }
            if (!matched && memcmp(e->name, target_sfn, 11) == 0) matched = true;
            if (!matched) { lfn_count = 0; continue; }
            if (e->attr & FAT32_ATTR_DIRECTORY) return 4;//验证不是目录
            //释放数据簇链
            uint32_t file_cluster = ((uint32_t)e->cluster_high << 16) | e->cluster_low;
            if (file_cluster != 0) {
                if (FAT32_FreeClusterChain(Info, Disk, Partition, file_cluster) != 0)return 3;
            }
            for (int j = 0; j < lfn_count; j++)buf[lfn_offsets[j]] = 0xE5;//标记LFN条目为已删除
            e->name[0] = 0xE5;//标记SFN条目为已删除
            //写回
            if (FAT32_WriteCluster(Info, Disk, Partition, current, buf) != 0) return 3;
            ATA_Flush_Cache(&Disk->channel, &Disk->is_master);
            return 0;
        }
df_next:
        if (FAT32_NextCluster(Info, Disk, Partition, current, &current) != 0) return 2;
        if (current >= FAT32_ENTRY_EOF) break;
    }
    return 1;
}
