/**
 * OS003 外围组件互连库
 * 2026/7/10 Liu Chunyi
 */

#ifndef _DRIVES_PCI_H_
#define _DRIVES_PCI_H_ 

#include <klib.h>

//PCI配置空间I/O端口
#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

//PCI标准寄存器偏移（每个4字节）
#define PCI_VENDOR_ID     0x00   // 16-bit Vendor ID
#define PCI_DEVICE_ID     0x02   // 16-bit Device ID
#define PCI_COMMAND       0x04   // 16-bit Command
#define PCI_STATUS        0x06   // 16-bit Status
#define PCI_REVISION      0x08   // 8-bit  Revision ID
#define PCI_PROG_IF       0x09   // 8-bit  Programming Interface
#define PCI_SUBCLASS      0x0A   // 8-bit  Subclass
#define PCI_CLASS         0x0B   // 8-bit  Class Code
#define PCI_HEADER_TYPE   0x0E   // 8-bit  Header Type
#define PCI_BAR0          0x10   // 32-bit Base Address Register 0
#define PCI_BAR1          0x14   // 32-bit Base Address Register 1
#define PCI_BAR2          0x18   // 32-bit Base Address Register 2
#define PCI_BAR3          0x1C   // 32-bit Base Address Register 3
#define PCI_BAR4          0x20   // 32-bit Base Address Register 4
#define PCI_BAR5          0x24   // 32-bit Base Address Register 5
#define PCI_SECONDARY_BUS 0x19   // 8-bit Secondary Bus Number（用于 PCI-PCI 桥）
#define PCI_IRQ_LINE      0x3C   // 8-bit IRQ Line

//存储控制器Class/Subclass
#define PCI_CLASS_MASS_STORAGE    0x01
#define PCI_SUBCLASS_IDE          0x01
#define PCI_SUBCLASS_SATA         0x06 
#define PCI_SUBCLASS_NVME         0x08
#define PCI_SUBCLASS_SCSI         0x00
#define PCI_SUBCLASS_RAID         0x04

//PCI显示设备Class Code
#define PCI_CLASS_DISPLAY       0x03
#define PCI_SUBCLASS_VGA        0x00
#define PCI_SUBCLASS_XGA        0x01
#define PCI_SUBCLASS_3D         0x02

//常见显卡 Vendor ID
#define VENDOR_QEMU     0x1234   //QEMU 虚拟显卡
#define VENDOR_VMWARE   0x15AD   //VMware
#define VENDOR_INTEL    0x8086   //Intel 核显
#define VENDOR_NVIDIA   0x10DE   //NVIDIA
#define VENDOR_AMD      0x1002   //AMD

//GPU类型
#define GPU_TYPE_UNKNOWN    0
#define GPU_TYPE_BOCHS      1
#define GPU_TYPE_VMWARE     2

//PCI设备信息结构体
typedef struct pci_device {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;
    uint8_t  header_type;
    uint32_t bar[6];       //前6个BAR
    uint8_t  irq_line;
} pci_device_t;

//显卡信息结构体
typedef struct gpu_device {
    uint8_t  bus;             // PCI 总线号
    uint8_t  device;          // PCI 设备号
    uint8_t  func;            // PCI 功能号
    uint16_t vendor_id;       // 厂商 ID
    uint16_t device_id;       // 设备 ID
    uint8_t  class_code;      // Class（应为 0x03）
    uint8_t  subclass;        // Subclass（0x00=VGA）
    uint8_t  prog_if;         // 编程接口
    uint8_t  revision;        // 修订版本
    uint32_t bar[6];          // 6 个 BAR 寄存器值
    uint8_t  irq_line;        // 中断线
    uint8_t  type;            // GPU 类型 (GPU_TYPE_*)
    uint32_t capabilities;    // 能力位图 (GPU_CAP_*)
    uint16_t vbe_version;     // Bochs VBE 版本号 (仅 Bochs)
} gpu_device_t;

/**
 * 读取 PCI 配置空间的一个 32 位寄存器
 * @param bus    总线号
 * @param device 设备号 (0-31)
 * @param func   功能号 (0-7)
 * @param offset 寄存器偏移 (0, 4, 8, ... 最多 252)
 * @return 32 位寄存器值
 */
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);
/**
 * 写入 PCI 配置空间的一个 32 位寄存器
 * @param bus    总线号
 * @param device 设备号
 * @param func   功能号
 * @param offset 寄存器偏移
 * @param value  要写入的值
 */
void pci_config_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value);
/**
 * 扫描所有 PCI 总线/设备/功能，查找匹配 class/subclass 的设备
 * @param class_code  要查找的 Class（如 0x01）
 * @param subclass    要查找的 Subclass（如 0x01 IDE，传 0xFF 表示任意）
 * @param result      结果数组
 * @param max_count   结果数组最大容量
 * @return 找到的设备数量
 */
int pci_find_storage_controllers(pci_device_t *result, int max_count);
_Bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t func);//检测指定 PCI 设备是否存在（VendorID != 0xFFFF 表示存在）
void PCI_Read_Device_Info(uint8_t bus, uint8_t device, uint8_t func, pci_device_t *info);//读取完整的 PCI 设备信息到结构体
void PCI_Print_Device(pci_device_t *dev);//打印一个 PCI 设备的信息

/**
 * 扫描 PCI 总线，查找所有显示控制器
 * @param result   结果数组
 * @param max_count 数组容量
 * @return 找到的显卡数量
 */
int pci_scan_gpu(gpu_device_t *result, int max_count);

#endif