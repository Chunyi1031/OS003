/**
 * OS003 硬件驱动-键盘
 * 2026/7/9
 */
#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_ 

#include <klib.h>

// Control 键
#define KEY_LeftCtrl   0x1D  // 左Control键
#define KEY_RightCtrl  0xE01D // 右Control键（扩展扫描码）

// Alt 键  
#define KEY_LeftAlt    0x38  // 左Alt键
#define KEY_RightAlt   0xE038 // 右Alt键（扩展扫描码）

// Shift 键
#define KEY_LeftShift  0x2A  // 左Shift
#define KEY_RightShift 0x36  // 右Shift

// 箭头键
#define KEY_UpArrow    0x48  // 上箭头键
#define KEY_DownArrow  0x50  // 下箭头键  
#define KEY_LeftArrow  0x4B  // 左箭头键
#define KEY_RightArrow 0x4D  // 右箭头键

#define KEYBOARD_BUFFER_SIZE 256

extern volatile char SYSTEM_KeyboardBuffer[KEYBOARD_BUFFER_SIZE];
extern volatile int kbbuffer_write_index;
extern volatile int kbbuffer_read_index;
extern volatile int kbbuffer_count;

extern const char keyboard_scan_to_ascii[128];//ASCII表小写
extern const char keyboard_scan_to_ascii_1[128];//ASCII表大写

int kbhit(void);//检查键盘是否可用
uint8_t read_scan_code(void);//读取扫描码
char _getkey(void);//直接读取一个按键
char _getkey_noblock(void);//直接读取一个按键(非阻塞)

char GetKey(void);//读取一个按键
char GetKey_NoBlock(void);//读取一个按键(非阻塞)

#endif