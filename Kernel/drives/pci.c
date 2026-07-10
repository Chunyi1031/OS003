#include <drives/pci.h>
#include <Print.h>

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(
        (1 << 31) |                 // 使能位，始终为 1
        ((uint32_t)bus << 16) |     // Bus 号
        ((uint32_t)device << 11) |  // Device 号 (0-31)
        ((uint32_t)func << 8) |     // Function 号 (0-7)
        (offset & 0xFC)             // 寄存器偏移（低 2 位必须为 0）
    );
    outl(PCI_CONFIG_ADDR, address); // 写地址
    return inl(PCI_CONFIG_DATA);    // 读数据
}

void pci_config_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)(
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC)
    );
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
}

_Bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t func) {
    uint32_t vendor = pci_config_read(bus, device, func, PCI_VENDOR_ID);
    return (vendor & 0xFFFF) != 0xFFFF;
}

/*DeepSeek-V4-Flash*/
void PCI_Read_Device_Info(uint8_t bus, uint8_t device, uint8_t func, pci_device_t *info) {
    info->bus    = bus;
    info->device = device;
    info->func   = func;
    //从偏移0x00读VendorID(16bit) + DeviceID(16bit)
    uint32_t id_reg = pci_config_read(bus, device, func, 0x00);
    info->vendor_id = (uint16_t)(id_reg & 0xFFFF);
    info->device_id = (uint16_t)((id_reg >> 16) & 0xFFFF);
    //从偏移 0x08 读 Revision(8bit) + ProgIF(8bit) + Subclass(8bit) + Class(8bit)
    uint32_t class_reg = pci_config_read(bus, device, func, 0x08);
    info->revision   = (uint8_t)(class_reg & 0xFF);
    info->prog_if    = (uint8_t)((class_reg >> 8) & 0xFF);
    info->subclass   = (uint8_t)((class_reg >> 16) & 0xFF);
    info->class_code = (uint8_t)((class_reg >> 24) & 0xFF);
    //从偏移 0x0C 读 Header Type
    uint32_t htype_reg = pci_config_read(bus, device, func, 0x0C);
    info->header_type = (uint8_t)((htype_reg >> 16) & 0xFF);
    //读 6 个 BAR（每个 32 位）
    for (int i = 0; i < 6; i++) {
        info->bar[i] = pci_config_read(bus, device, func, 0x10 + i * 4);
    }
    //IRQ Line
    uint32_t irq_reg = pci_config_read(bus, device, func, 0x3C);
    info->irq_line = (uint8_t)(irq_reg & 0xFF);
}

// 遍历所有 Bus(0)、Device(0-31)、Function(0-7)
// 如果设备是 Class=0x01（大容量存储控制器），记录到 result 数组
// 返回值：找到的控制器数量
int pci_find_storage_controllers(pci_device_t *result, int max_count) {
    int count = 0;

    // 目前只扫描 Bus 0。如果需要扫描更多 Bus（有 PCI-PCI 桥），后续可以扩展
    for (uint8_t dev = 0; dev < 32; dev++) {
        for (uint8_t func = 0; func < 8; func++) {
            if (!pci_device_exists(0, dev, func)) {
                // 注意：如果 func=0 不存在，跳过整个 device
                // 如果 func=0 存在但 header_type 是单功能设备(bit7=0)，func>0 就不用扫了
                if (func == 0) {
                    // 读 header type 判断是不是多功能设备
                    uint32_t htype_reg = pci_config_read(0, dev, 0, 0x0C);
                    uint8_t header_type = (uint8_t)((htype_reg >> 16) & 0xFF);
                    if (!(header_type & 0x80)) {
                        // bit 7 = 0 → 单功能设备，不需要扫 func > 0
                        break;
                    }
                }
                continue;
            }

            // 读取 Class Code
            uint32_t class_reg = pci_config_read(0, dev, func, 0x08);
            uint8_t class_code = (uint8_t)((class_reg >> 24) & 0xFF);
            uint8_t subclass   = (uint8_t)((class_reg >> 16) & 0xFF);

            // 只关注大容量存储控制器 (Class = 0x01)
            if (class_code != PCI_CLASS_MASS_STORAGE) {
                // 如果是单功能设备，不需要继续扫 func>0
                if (func == 0) {
                    uint32_t htype_reg = pci_config_read(0, dev, 0, 0x0C);
                    uint8_t header_type = (uint8_t)((htype_reg >> 16) & 0xFF);
                    if (!(header_type & 0x80)) break;
                }
                continue;
            }

            // 找到了！记录设备信息
            if (count < max_count) {
                PCI_Read_Device_Info(0, dev, func, &result[count]);
                count++;
            }

            // 单功能设备不再扫 func>0
            if (func == 0) {
                uint32_t htype_reg = pci_config_read(0, dev, 0, 0x0C);
                uint8_t header_type = (uint8_t)((htype_reg >> 16) & 0xFF);
                if (!(header_type & 0x80)) break;
            }
        }
    }

    return count;
}

void PCI_Print_Device(pci_device_t *dev) {
    // Subclass 对应的文字描述
    const char *subclass_name;
    switch (dev->subclass) {
        case PCI_SUBCLASS_IDE:  subclass_name = "IDE Controller";  break;
        case PCI_SUBCLASS_SATA: subclass_name = "SATA/AHCI Ctrl"; break;
        case PCI_SUBCLASS_NVME: subclass_name = "NVMe Ctrl";      break;
        case PCI_SUBCLASS_SCSI: subclass_name = "SCSI Ctrl";      break;
        case PCI_SUBCLASS_RAID: subclass_name = "Raid Ctrl";      break;
        default:                subclass_name = "Storage Ctrl";    break;
    }

    printf("  %X:%X  %s  Bus%d Dev%d Func%d\n",
        dev->vendor_id, dev->device_id, subclass_name,
        dev->bus, dev->device, dev->func);
}
/*DeepSeek-V4-Flash-END*/

//PCI扫描
int pci_scan_gpu(gpu_device_t *result, int max_count) {
    int count = 0;
    for (uint8_t dev = 0; dev < 32; dev++) {
        for (uint8_t func = 0; func < 8; func++) {
            if (!pci_device_exists(0, dev, func)) {
                if (func == 0) {
                    uint32_t htype = pci_config_read(0, dev, 0, 0x0C);
                    if (!((htype >> 16) & 0x80)) break;
                }
                continue;
            }
            uint32_t class_reg = pci_config_read(0, dev, func, 0x08);
            if (((class_reg >> 24) & 0xFF) != PCI_CLASS_DISPLAY) {
                if (func == 0) {
                    uint32_t htype = pci_config_read(0, dev, 0, 0x0C);
                    if (!((htype >> 16) & 0x80)) break;
                }
                continue;
            }
            if (count >= max_count) return count;
            pci_device_t pci;
            PCI_Read_Device_Info(0, dev, func, &pci);
            result[count].bus        = pci.bus;
            result[count].device     = pci.device;
            result[count].func       = pci.func;
            result[count].vendor_id  = pci.vendor_id;
            result[count].device_id  = pci.device_id;
            result[count].class_code = pci.class_code;
            result[count].subclass   = pci.subclass;
            result[count].prog_if    = pci.prog_if;
            result[count].revision   = pci.revision;
            result[count].irq_line   = pci.irq_line;
            for (int i = 0; i < 6; i++)
                result[count].bar[i] = pci.bar[i];
            result[count].type         = GPU_TYPE_UNKNOWN;
            result[count].capabilities = 0;
            result[count].vbe_version  = 0;
            count++;
            if (func == 0) {
                uint32_t htype = pci_config_read(0, dev, 0, 0x0C);
                if (!((htype >> 16) & 0x80)) break;
            }
        }
    }
    return count;
}
