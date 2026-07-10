#include <fs/fs.h>
#include <Print.h>
#include <mm/pmm.h>

sys_part_t SystemPartition = {0};

STATUS InitFileSystem(const uint8_t guid[16]){
    sys_part_t result;
    memset(&result,0,sizeof(sys_part_t));
    //寻找硬盘
    result.Disk = FindFirstATADisk();
    ata_disk_t disk_zero = {0};
    if(memcmp(&result.Disk,&disk_zero,sizeof(ata_disk_t)) == 0)return 1;
    //识别分区
    STATUS Status = FindPartition(&result.Disk.channel,result.Disk.is_master,guid,&result.Partition);
    if(Status != 0)return 20+Status;
    //获取FAT32分区信息
    Status = FAT32_GetInfo(&result.Disk,&result.Partition,&result.PartitionInfo);
    if(Status != 0)return 30+Status;
    memcpy(&SystemPartition,&result,sizeof(sys_part_t));
    return 0;
}

file_t OpenFile(char* path,int mode){
    file_t result = {0};
    if(!path || path[0] == '\0') return result;
    //跳过开头的 '/'
    char* p = path;
    while(*p == '/') p++;
    //如果路径只有 "/",返回根目录
    if(*p == '\0'){
        //为根目录创建一个假的目录项，方便调用方统一检查 dir.entry
        fat32_dir_entry_t* root_entry = (fat32_dir_entry_t*)Pmm_Malloc(1);
        if(root_entry){
            memset(root_entry, 0, sizeof(fat32_dir_entry_t));
            root_entry->attr = FAT32_ATTR_DIRECTORY;
            root_entry->cluster_low = (uint16_t)(SystemPartition.PartitionInfo.root_cluster & 0xFFFF);
            root_entry->cluster_high = (uint16_t)(SystemPartition.PartitionInfo.root_cluster >> 16);
        }
        result.entry = root_entry;
        result.dir_cluster = SystemPartition.PartitionInfo.root_cluster;
        result.status = mode;
        return result;
    }
    //第一层根目录,设置当前簇为根目录簇
    uint32_t current_cluster = SystemPartition.PartitionInfo.root_cluster;
    fat32_dir_entry_t last_entry;
    _Bool found_any = 0;
    char* last_name_start = p;     //记录最后一个分量的起始位置
    char component[FAT32_MAX_NAME];
    //逐层遍历路径
    while(*p){
        char* component_start = p;   //记录当前分量在原始路径中的位置
        int comp_idx = 0;
        //提取当前路径分量
        while(*p && *p != '/'){
            if(comp_idx < FAT32_MAX_NAME - 1)
                component[comp_idx++] = *p;
            p++;
        }
        component[comp_idx] = '\0';
        //跳过连续的分隔符
        if(comp_idx == 0){
            if(*p == '/'){ p++; continue; }
            break;
        }
        //在当前目录中查找该分量
        STATUS status = FAT32_FindEntryAny(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,current_cluster,component,&last_entry);
        if(status != 0) return result;//未找到
        found_any = 1;
        last_name_start = component_start;   //更新为当前分量起始位置
        //如果后面还有路径分量,当前必须是目录
        if(*p == '/'){
            if(!(last_entry.attr & FAT32_ATTR_DIRECTORY))return result;//不是目录
            current_cluster = ((uint32_t)last_entry.cluster_high << 16) | last_entry.cluster_low;
            p++;//跳到下一分量
        }
    }
    //找到了目标文件/目录,填入file_t
    if(found_any){
        fat32_dir_entry_t* entry_ptr = (fat32_dir_entry_t*)Pmm_Malloc(1);
        if(!entry_ptr) return result;
        *entry_ptr = last_entry;
        result.entry = entry_ptr;
        result.dir_cluster = current_cluster;//父目录簇号
        result.name = last_name_start;
        result.status = mode;
    }
    return result;
}

void CloseFile(file_t *file){
    memset(file,0,sizeof(file_t));
    file->status = FILE_CLOSED;
}

int ReadFile(file_t *file,uint8_t* buffer){
    if((file->status != FILE_READ) && (file->status != (FILE_READ | FILE_WRITE)))return -1;//检查文件状态
    return FAT32_ReadFile(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,file->entry,buffer);
}

int WriteFile(file_t *file,uint8_t* buffer,size_t size){
    if((file->status != FILE_WRITE) && (file->status != (FILE_READ | FILE_WRITE)))return -1;//检查文件状态
    return FAT32_WriteFile(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,file->dir_cluster,file->entry,buffer,size);
}

int AppendFile(file_t *file,uint8_t* buffer,size_t size){
    if((file->status != FILE_WRITE) && (file->status != (FILE_READ | FILE_WRITE)))return -1;//检查文件状态
    return FAT32_AppendFile(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,file->dir_cluster,file->entry,buffer,size);
}

STATUS RenameFile(char* path,char* new_name){
    file_t file = OpenFile(path,FILE_CLOSED);//"打开"文件,仅获取必要信息
    //检查文件名是否合规
    for (int i = 0; new_name[i] != '\0'; i++) {
        if (new_name[i] == '/')return -1;
    }
    return FAT32_RenameFile(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,file.dir_cluster,file.name,new_name);//重命名
}

fat32_list_entry_t* ListDirectory(char* path,int *count,int *out_pages){
    //根目录特殊处理
    if(strcmp(path,"/") == 0)return FAT32_ListDirectory(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,\
                                                        SystemPartition.PartitionInfo.root_cluster,count,out_pages);
    file_t dir = OpenFile(path,FILE_CLOSED);//"打开"文件，仅获取必要信息
    return FAT32_ListDirectory(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,\
                               (((uint32_t)dir.entry->cluster_high << 16) | dir.entry->cluster_low),count,out_pages);
}

STATUS CreateFile(char* path){
    if(!path || path[0] != '/') return 6;
    //找到最后一个 '/'
    char* last_slash = path;
    for(char* pp = path; *pp; pp++){
        if(*pp == '/') last_slash = pp;
    }
    char* filename = last_slash + 1;
    if(*filename == '\0') return 6;//路径以/结尾，没有文件名
    //截断路径，获取父目录
    char saved = *last_slash;
    *last_slash = '\0';
    file_t dir = OpenFile((last_slash == path) ? "/" : path, FILE_CLOSED);
    *last_slash = saved;    //恢复路径
    if(dir.entry == NULL || !(dir.entry->attr & FAT32_ATTR_DIRECTORY)) return 6;
    uint32_t dir_cluster = ((uint32_t)dir.entry->cluster_high << 16) | dir.entry->cluster_low;
    Pmm_Free(dir.entry,1);
    return FAT32_CreateFile(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,dir_cluster,filename);
}

STATUS CreateDir(char* path){
    if(!path || path[0] != '/') return 6;
    //找到最后一个 '/'
    char* last_slash = path;
    for(char* pp = path; *pp; pp++){
        if(*pp == '/') last_slash = pp;
    }
    char* dirname = last_slash + 1;
    if(*dirname == '\0') return 6;//路径以/结尾
    //截断路径，获取父目录
    char saved = *last_slash;
    *last_slash = '\0';
    file_t dir = OpenFile((last_slash == path) ? "/" : path, FILE_CLOSED);
    *last_slash = saved;    //恢复路径
    if(dir.entry == NULL || !(dir.entry->attr & FAT32_ATTR_DIRECTORY)) return 6;
    uint32_t dir_cluster = ((uint32_t)dir.entry->cluster_high << 16) | dir.entry->cluster_low;
    Pmm_Free(dir.entry,1);
    return FAT32_CreateDir(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,dir_cluster,dirname);
}

STATUS DeleteFile(char* path){
    if(!path || path[0] != '/') return 6;
    char* last_slash = path;
    for(char* pp = path; *pp; pp++){
        if(*pp == '/') last_slash = pp;
    }
    char* filename = last_slash + 1;
    if(*filename == '\0') return 6;
    char saved = *last_slash;
    *last_slash = '\0';
    file_t dir = OpenFile((last_slash == path) ? "/" : path, FILE_CLOSED);
    *last_slash = saved;
    if(dir.entry == NULL || !(dir.entry->attr & FAT32_ATTR_DIRECTORY)) return 6;
    uint32_t dir_cluster = ((uint32_t)dir.entry->cluster_high << 16) | dir.entry->cluster_low;
    Pmm_Free(dir.entry,1);
    return FAT32_DeleteFile(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,dir_cluster,filename);
}

STATUS RemoveDir(char* path){
    if(!path || path[0] != '/') return 6;
    char* last_slash = path;
    for(char* pp = path; *pp; pp++){
        if(*pp == '/') last_slash = pp;
    }
    char* dirname = last_slash + 1;
    if(*dirname == '\0') return 6;
    char saved = *last_slash;
    *last_slash = '\0';
    file_t dir = OpenFile((last_slash == path) ? "/" : path, FILE_CLOSED);
    *last_slash = saved;
    if(dir.entry == NULL || !(dir.entry->attr & FAT32_ATTR_DIRECTORY)) return 6;
    uint32_t dir_cluster = ((uint32_t)dir.entry->cluster_high << 16) | dir.entry->cluster_low;
    Pmm_Free(dir.entry,1);
    return FAT32_RemoveDir(&SystemPartition.PartitionInfo,&SystemPartition.Disk,&SystemPartition.Partition,dir_cluster,dirname);
}
