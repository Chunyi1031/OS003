#include <klib.h>

// ── 简单 LCG 伪随机数生成器 ──
static uint64_t rand_seed = 1;

int rand(void) {
    rand_seed = rand_seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return (int)(rand_seed >> 33);
}

void srand(uint64_t seed) {
    rand_seed = seed;
}

//将无符号整数转为十六进制字符串
size_t itoa_hex(uint64_t value, char *buf) {
    char temp[32];
    int i = 0;
    // 特殊处理 0
    if (value == 0) {
        buf[0] = '0';
        buf[1] = 'x';
        buf[2] = '0';
        buf[3] = '\0';
        return 3;
    }
    // 逆序生成 hex
    while (value > 0) {
        int digit = value & 0xF;
        temp[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        value >>= 4;
    }
    // 写 "0x"
    buf[0] = '0';
    buf[1] = 'x';
    int j = 2;
    // 反向拷贝
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
    return (size_t)j;
}
//将有符号整数转为十进制字符串
size_t itoa_dec(int64_t value, char *buf) {
    char temp[32];
    int i = 0;
    uint64_t num;
    int negative = 0;
    if (value < 0) {
        negative = 1;
        num = (uint64_t)(-value);
    } else {
        num = (uint64_t)value;
    }
    // 特殊处理 0
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    // 逆序生成数字
    while (num > 0) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    int j = 0;
    // 先写负号（如果有）
    if (negative) {
        buf[j++] = '-';
    }
    // 再反向拷贝数字
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';

    return (size_t)j;
}
size_t utoa_dec(uint64_t value, char *buf) {
    char temp[32];
    int i = 0;
    
    // 特殊处理 0
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    
    // 逆序生成数字
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    // 反向拷贝到输出缓冲区
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
    
    return (size_t)j;
}

//...的最后一个字符
char LastCharOf(char* s){
    size_t len = strlen(s) - 1;
    return s[len];
}
//字符在字符串中出现的次数
size_t strcount(const char *str, char ch) {
    size_t count = 0;
    if (str == NULL) {
        return 0;
    }
    //遍历字符串
    while (*str != '\0') {
        if (*str == ch) {
            count++;
        }
        str++;
    }
    return count;
}
//反转字符串
void reverse(char *str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

//From Linux 5.2.20  lib/string.c 512
size_t strlen(const char *s) {
    const char *sc;

	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}
//From Linux 5.2.20  lib/string.c 529
size_t strnlen(const char *s, size_t count)
{
	const char *sc;

	for (sc = s; count-- && *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];  // 返回差值
        }
    }
    return 0;  // 完全相同
}

int strcmp(char* str1,char* str2){
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    if(len1 == 0 && len2 == 0) return 0;
    if((len1 == 0)||(len2 == 0)) return 1;
    if(len1 >= len2){
        for(int i = 0;i < len2;i ++){
            if(str1[i] != str2[i]){
                return 2;
            }
        }
        if(len1 != len2) return 1;
        return 0;
    }else{
        for(int i = 0;i < len1;i ++){
            if(str1[i] != str2[i]){
                return 2;
            }
        }
        return 1;
    }
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}

//From Linux 5.2.20  lib/string.c 729
void *memset(void *s, int c, size_t count)
{
	char *xs = (char *)s;

	while (count--)
		*xs++ = c;
	return s;
}
//From Linux 5.2.20  lib/string.c 772
void *memset16(uint16_t *s, uint16_t v, size_t count)
{
	uint16_t *xs = s;

	while (count--)
		*xs++ = v;
	return s;
}

void *memcpy(void *dest, const void *src, size_t count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    
    if (n == 0) {
        return 0;
    }
    
    return (unsigned char)*s1 - (unsigned char)*s2;
}

uint16_t get_bits(uint16_t value, uint8_t start, uint8_t count) {
    return (value >> start) & ((1 << count) - 1);
}
