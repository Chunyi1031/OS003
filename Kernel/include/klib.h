/**
 *  OS003 Kernel Library
 * 内核库，定义内核运行所需的基础函数
 * 2026/7/9 Liu Chunyi
 */

#ifndef _KERNEL_LIB_H_
#define _KERNEL_LIB_H_

#include <types.h>
#include <kstring.h>
#include <io.h>

#define OS_NAME "OS003"

//引导程序传递的数据结构
//屏幕数据
typedef struct SCREEN_DATA{
    uint64_t FrameBuffer;
    uint16_t SW;
    uint16_t SH;
}SCREEN_DATA;
//内存映射
typedef struct MEMORY_MAP{
    uint64_t MapSize;
    uint64_t DescriptorSize;
    uint32_t DescriptorVersion;
    void* StackAddr;
    void* Buffer;
    void* OsDescAddr;
}UEFI_MEMORY_MAP;
//总结构体
typedef struct BOOT_SHARE{
    SCREEN_DATA* ScreenData;
    uint64_t CPUF;
    uint32_t FONTADDR;
    void *RSDP;
    UEFI_MEMORY_MAP* MemoryMap;
} BOOT_SHARE;

extern uint64_t SYSTEM_CPU_Frequency;//全局CPU频率
extern BOOT_SHARE* SYSTEM_BootShare;//全局引导数据

void SYSTEM_STOP();//停机
void SYSTEM_DEBUG();

uint64_t Get_CPU_Frequency();//获取CPU频率

void delay_ms(uint32_t ms);//延迟毫秒
void delay_seconds(uint32_t seconds);//延迟秒

//串口函数声明
#define SERIAL_BAUD 115200
#define SERIAL_COM1_PORT 0x3F8
#define SERIAL_COM2_PORT 0x2F8
#define SERIAL_COM3_PORT 0x3E8
STATUS serial_init(uint16_t port);//初始化串口
int is_transmit_empty(uint16_t port);//检查串口是否准备好发送数据
void serial_putchar(uint16_t port, char c);//发送单个字符到串口
void serial_putstr(uint16_t port, const char* str);//发送字符串到串口
void serial_puthex(uint16_t port, uint64_t v);//发送16进制数到串口

size_t strlen(const char *s);//计算字符串长度
size_t itoa_dec(int64_t value, char *buf);//将有符号整数转为十进制字符串
size_t itoa_hex(uint64_t value, char *buf);//将无符号整数转为十六进制字符串
size_t utoa_dec(uint64_t value, char *buf);//将无符号整数转为十进制字符串
char LastCharOf(char* s);//...的最后一个字符
size_t strcount(const char *str, char ch);//字符在字符串中出现的次数
void reverse(char *str, int length);//反转字符串
int strcmp(char* str1,char* str2);//比较字符串
int memcmp(const void* s1, const void* s2, size_t n);//逐字节比较两块内存
/**
 * strnlen - Find the length of a length-limited string
 * @s: The string to be sized
 * @count: The maximum number of bytes to search
 */
size_t strnlen(const char *s, size_t count);
/**
 * memcpy - Copy one area of memory to another
 * @dest: Where to copy to
 * @src: Where to copy from
 * @count: The size of the area.
 *
 * You should not use this function to access IO space, use memcpy_toio()
 * or memcpy_fromio() instead.
 */
void *memcpy(void *dest, const void *src, size_t count);
/**
 * memset - Fill a region of memory with the given value
 * @s: Pointer to the start of the area.
 * @c: The byte to fill the area with
 * @count: The size of the area.
 *
 * Do not use memset() to access IO space, use memset_io() instead.
 */
void *memset(void *s, int c, size_t count);
/**
 * memset16() - Fill a memory area with a uint16_t
 * @s: Pointer to the start of the area.
 * @v: The value to fill the area with
 * @count: The number of values to store
 *
 * Differs from memset() in that it fills with a uint16_t instead
 * of a byte.  Remember that @count is the number of uint16_ts to
 * store, not the number of bytes.
 */
void *memset16(uint16_t *s, uint16_t v, size_t count);

char *strcpy(char *dest, const char *src);
int strncmp(const char *s1, const char *s2, size_t n);
uint16_t get_bits(uint16_t value, uint8_t start, uint8_t count);//从uint16中提取指定位段

int rand(void);
void srand(uint64_t seed);
void UpdateCursor(uint32_t color);

#endif