#include <Keyboard.h>

_Bool shift_pressed = false;
uint8_t* font_data = NULL;
volatile int kbbuffer_write_index = 0;
volatile int kbbuffer_read_index = 0;
volatile int kbbuffer_count = 0;

const char scan_to_ascii[128] = {
    0,  0, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};
const char scan_to_ascii_1[128] = {
    0,  0, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

int kbhit(void) {
    return (inb(0x64) & 0x01);
}
uint8_t read_scan_code(void) {
    while (!kbhit());//等待有数据可读
    return inb(0x60);//返回扫描码
}
char _getkey(void) {
    while (1) {
        uint8_t scancode = read_scan_code();
        //处理Shift键状态
        if (scancode == KEY_LeftShift || scancode == KEY_RightShift) { // Shift按下
            shift_pressed = 1;
            continue;
        } else if (scancode == (KEY_LeftShift | 0x80) || scancode == (KEY_RightShift | 0x80)) { // Shift释放
            shift_pressed = 0;
            continue;
        }
        //忽略其他按键的释放事件
        if (scancode & 0x80) {
            continue;
        }
        //根据Shift状态选择转换表
        char c = shift_pressed ? scan_to_ascii_1[scancode] : scan_to_ascii[scancode];
        return c != 0 ? c : (char)0;
    }
}
char _getkey_noblock(void) {
    while(1){
        uint8_t scancode;
        scancode = inb(0x60);
        //处理Shift键状态
        if (scancode == KEY_LeftShift || scancode == KEY_RightShift) {//Shift按下
            shift_pressed = 1;
            return 0;
        } else if (scancode == (KEY_LeftShift | 0x80) || scancode == (KEY_RightShift | 0x80)) {//Shift释放
            shift_pressed = 0;
            return 0;
        }
        //忽略其他按键的释放事件
        if (scancode & 0x80) {
            return 0;
        }
        //根据Shift状态选择转换表
        char c = shift_pressed ? scan_to_ascii_1[scancode] : scan_to_ascii[scancode];
        return c != 0 ? c : (char)0;
    }
}

char GetKey(void){
    char key = 0;
    //等待键盘中断响应
    while(kbbuffer_count == 0);
    asm("cli");
    key = SYSTEM_KeyboardBuffer[kbbuffer_read_index];
    SYSTEM_KeyboardBuffer[kbbuffer_read_index] = 0;
    kbbuffer_read_index = (kbbuffer_read_index + 1) % KEYBOARD_BUFFER_SIZE;
    kbbuffer_count --;
    asm("sti");
    return key;
}

char GetKey_NoBlock(void){
    char key = 0;
    asm("cli");
    if(kbbuffer_count != 0){
        key = SYSTEM_KeyboardBuffer[kbbuffer_read_index];
        SYSTEM_KeyboardBuffer[kbbuffer_read_index] = 0;
        kbbuffer_read_index = (kbbuffer_read_index + 1) % KEYBOARD_BUFFER_SIZE;
        kbbuffer_count --;
    }
    asm("sti");
    return key;
}