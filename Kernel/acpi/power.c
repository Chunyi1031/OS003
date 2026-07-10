#include <ACPI.h>
#include <Print.h>
#include <Thread.h>

int poweroff_acpi(){
    FADT_DESCRIPTOR *FADT = SYSTEM_FADT_ADDR;
    if(!FADT){
        out_error("ACPI FADT not found!\n");
        return 1;
    }
    //获取PM1a控制块地址
    uint32_t pm1a_cnt_addr = FADT->PM1a_CNT_BLK;
    if(pm1a_cnt_addr == 0) {
        out_error("PM1a control block not found\n");
        return 2;
    }
    //在FADT中查找S5睡眠类型
    uint8_t* fadt_bytes = (uint8_t*)FADT;
    uint8_t slp_typa = fadt_bytes[0x8C];//SLP_TYPa
    uint8_t slp_typb = fadt_bytes[0x8D];//SLP_TYPb
    uint8_t slp_en = 0x1;//SLP_EN 位
    uint16_t pm1a_value = 0;
    pm1a_value |= (1 << 9);//设置 SCI_EN
    pm1a_value |= ((slp_typa & 0x5) << 10);//设置 S5 睡眠类型
    pm1a_value |= (slp_en << 13);//设置 SLP_EN 位以触发睡眠/关机
    if (FADT->PM1b_CNT_BLK)outw(FADT->PM1b_CNT_BLK, pm1a_value);
    outw(pm1a_cnt_addr, pm1a_value);//写入 PM1a 控制寄存器
    delay_ms(10);
    pm1a_value = ((slp_typa & 0x7) << 10) | (1 << 13);
    outw(pm1a_cnt_addr, pm1a_value);
    return 3;
}

int reboot_acpi(){
    FADT_DESCRIPTOR *FADT = SYSTEM_FADT_ADDR;
    if(!FADT){
        out_error("ACPI FADT not found!\n");
        return 1;
    }
    outb(FADT->RESET_REG.Address, FADT->RESET_VALUE);
    delay_ms(10);
    return 0;
}

void reboot_legacy(){
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);//复位
    io_wait();
}

void SYSTEM_SHUTDOWN(){
    CleanAllThreads();
    print("The machine will turn off the power\n",COLOR_CYAN);
    delay_ms(200);
    disable_interrupts();
    poweroff_acpi();
    out_waring("ACPI power off failed, try legacy power off...\n");
    delay_ms(500);
    outw(0xB004, 0x2000);
    outw(0x604, 0x2000);
    outw(0x4004, 0x3400);
    outw(0x600, 0x34);
    io_wait();
    reboot_legacy();
    out_error("Shutdown failed!\n");
    enable_interrupts();
}

void SYSTEM_REBOOT(){
    CleanAllThreads();
    print("The machine will restart\n",COLOR_CYAN);
    delay_ms(200);
    disable_interrupts();
    reboot_acpi();
    out_waring("ACPI reboot failed, try legacy reboot...\n");
    delay_ms(500);
    reboot_legacy();
    out_error("Reboot failed!\n");
    enable_interrupts();
}