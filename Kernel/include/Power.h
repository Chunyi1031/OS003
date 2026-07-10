#ifndef _POWER_H_
#define _POWER_H_ 

#include <klib.h>

int poweroff_acpi();//ACPI关机(强制断电)
int reboot_acpi();//ACPI重启（强制重启）
void reboot_legacy();//传统Legacy关机（强制复位）

void SYSTEM_SHUTDOWN();//系统关机
void SYSTEM_REBOOT();//系统重启

#endif