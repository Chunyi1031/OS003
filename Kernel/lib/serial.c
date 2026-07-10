#include <klib.h>

_Bool SerialIsInit = false;

//串口初始化
STATUS serial_init(uint16_t port){
    outb(port + 1, 0x00);//禁用中断
    outb(port + 3, 0x80);//启用 DLAB
    outb(port + 0, (1843200 / (16*SERIAL_BAUD)));//设置波特率
    outb(port + 1, 0x00);
    outb(port + 3, 0x03);//8位数据
    outb(port + 2, 0xC7);//启用 FIFO
    outb(port + 4, 0x0B);
    outb(port + 4, 0x1E);
    outb(port + 0, 0xAE);
    if (inb(port + 0) != 0xAE) {
        return 1;
    }
    outb(port + 4, 0x0F);
    SerialIsInit = true;
    return STATUS_SUCCESS;
}
// 检查串口是否准备好发送数据
int is_transmit_empty(uint16_t port) {
    return inb(port + 5) & 0x20;
}
// 发送单个字符到串口
__attribute__((__externally_visible__))
void serial_putchar(uint16_t port, char c) {
    if(SerialIsInit){
        while (!is_transmit_empty(port));  // 等待发送缓冲区为空
        outb(port, c);  // 发送字符
    }
}
// 发送字符串到串口
void serial_putstr(uint16_t port, const char* str) {
    while (*str) {
        serial_putchar(port, *str++);
    }
}

// 发送16进制数到串口
void serial_puthex(uint16_t port, uint64_t v) {
    char buf[19];
    int i = 18;
    buf[i--] = 0;
    if (v == 0) { buf[i--] = '0'; }
    while (v && i >= 0) {
        buf[i--] = "0123456789ABCDEF"[v & 0xF];
        v >>= 4;
    }
    buf[i] = '0'; buf[1] = 'x';
    serial_putstr(port, &buf[i]);
}