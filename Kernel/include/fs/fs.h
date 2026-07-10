/**
 * OS003 文件系统库
 * ATA GPT
 * FAT32
 */
#ifndef _FS_H_
#define _FS_H_

#include <klib.h>
#include <drives/ata.h>
#include <drives/gpt.h>
#include "fat32.h"

//系统分区描述符
typedef struct sys_part_t {
    ata_disk_t Disk;                    //硬盘
    gpt_partition_entry_t Partition;    //分区GPT表项
    fat32_info_t PartitionInfo;         //分区信息
} sys_part_t;

//文件描述符
typedef struct file_t {
    fat32_dir_entry_t* entry;       //文件目录项
    uint32_t dir_cluster;           //父目录簇号
    char* name;                     //完整文件名
    int status;                     //状态
} file_t;

#define FILE_CLOSED 0
#define FILE_READ   1
#define FILE_WRITE  2

extern sys_part_t SystemPartition;//系统分区

/**
 * 初始化系统文件系统
 * @param guid 分区类型GUID，建议使用ESP分区
 * @return 错误码，0:成功 1:未找到硬盘 18-24:FindPartition错误（错误码-20） 30-32:FAT32_GetInfo错误（错误码-30）
 */
STATUS InitFileSystem(const uint8_t guid[16]);

/**
 * 打开一个文件
 * @param path 文件路径
 * @param mode 打开模式
 * @return 文件描述符
 */
file_t OpenFile(char* path,int mode);

/**
 * 关闭文件
 * @param file 文件描述符
 */
void CloseFile(file_t *file);

/**
 * 读取文件
 * @param file 文件描述符
 * @param buffer 数据缓冲区
 * @return 大于0时，为读取的字节数，为0时，读取失败，为-1时，文件打开模式错误
 */
int ReadFile(file_t *file,uint8_t* buffer);

/**
 * 写入文件（覆盖写入）
 * @param file 文件描述符
 * @param buffer 数据缓冲区
 * @param size 写入大小
 * @return 大于0时，为写入的字节数，为0时，写入失败，为-1时，文件打开模式错误
 */
int WriteFile(file_t *file,uint8_t* buffer,size_t size);

/**
 * 写入文件（追加写入）
 * @param file 文件描述符
 * @param buffer 数据缓冲区
 * @param size 写入大小
 * @return 大于0时，为写入的字节数，为0时，写入失败，为-1时，文件打开模式错误
 */
int AppendFile(file_t *file,uint8_t* buffer,size_t size);

/**
 * 修改文件名
 * @param path 文件路径（完整）
 * @param new_name 新文件名（不能包含/）
 * @return 0=成功, 1=未找到, 2=读错误, 3=写错误, 4=新文件名已存在 -1=文件名违规
 */
STATUS RenameFile(char* path,char* new_name);

/**
 * 列出指定目录
 * @param path      目录路径
 * @param count     文件/目录数量指针
 * @param out_pages 目录项列表所占的页数
 * @return 目录项列表
 */
fat32_list_entry_t* ListDirectory(char* path,int *count,int *out_pages);

/**
 * 创建文件
 * @param path 完整路径
 * @return 0=成功, 6=目录不存在, 其他=FAT32_CreateFile错误码
 */
STATUS CreateFile(char* path);

/**
 * 创建目录
 * @param path 完整路径
 * @return 0=成功, 6=目录不存在, 其他=FAT32_CreateDir错误码
 */
STATUS CreateDir(char* path);

/**
 * 删除文件
 * @param path 完整路径
 * @return 0=成功, 6=目录不存在, 其他=FAT32_DeleteFile错误码
 */
STATUS DeleteFile(char* path);

/**
 * 删除目录
 * @param path 完整路径
 * @return 0=成功, 6=目录不存在, 其他=FAT32_RemoveDir错误码
 */
STATUS RemoveDir(char* path);

#endif