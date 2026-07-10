#ifndef _STRING_H_
#define _STRING_H_

#include <klib.h>

size_t strlen(const char *s);//计算字符串长度
size_t itoa_dec(int64_t value, char *buf);//将有符号整数转为十进制字符串
size_t itoa_hex(uint64_t value, char *buf);//将无符号整数转为十六进制字符串
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

#endif