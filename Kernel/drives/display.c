#include <drives/display.h>
#include <drives/pci.h>
#include <Draw.h>
#include <Print.h>
#include <Memory.h>

display_driver_t *g_display_driver = NULL;

//Bochs VBE驱动---------------------------------------------------------------------------------------------

//Bochs VBE I/O
#define VBE_INDEX  0x01CE
#define VBE_DATA   0x01CF
#define VBE_REG_ID         0
#define VBE_REG_XRES       1
#define VBE_REG_YRES       2
#define VBE_REG_BPP        3
#define VBE_REG_ENABLE     4
#define VBE_REG_VIRT_WIDTH 6
#define VBE_ENABLE      0x01
#define VBE_LFB_ENABLED 0x40
#define VBE_NOCLEAR     0x80

 uint16_t vbe_r(uint16_t reg) {
    outw(VBE_INDEX, reg);
    return inw(VBE_DATA);
}
void vbe_w(uint16_t reg, uint16_t val) {
    outw(VBE_INDEX, reg);
    outw(VBE_DATA, val);
}

//绘制基元
void bochs_pixel(uint32_t x, uint32_t y, uint32_t color) {
    display_info_t *info = &g_display_driver->info;
    if (x < info->width && y < info->height)info->framebuffer[y * info->width + x] = color;
}

void bochs_fill_rect(uint32_t x, uint32_t y,uint32_t w, uint32_t h, uint32_t color) {
    display_info_t *info = &g_display_driver->info;
    if (x >= info->width || y >= info->height) return;
    if (x + w > info->width)  w = info->width - x;
    if (y + h > info->height) h = info->height - y;
    if (w == 0 || h == 0) return;
    for (uint32_t row = 0; row < h; row++) {
        uint32_t *line = &info->framebuffer[(y + row) * info->width + x];
        uint32_t cnt = w;
        __asm__ volatile("rep stosl" : "+D"(line), "+c"(cnt) : "a"(color) : "memory");
    }
}

void bochs_clear(uint32_t color) {
    bochs_fill_rect(0, 0, g_display_driver->info.width, g_display_driver->info.height, color);
}

void bochs_hline(uint32_t x, uint32_t y,uint32_t w, uint32_t color) {
    display_info_t *info = &g_display_driver->info;
    if (y >= info->height) return;
    if (x >= info->width)  return;
    if (x + w > info->width) w = info->width - x;
    if (w == 0) return;
    uint32_t *start = &info->framebuffer[y * info->width + x];
    uint32_t cnt = w;
    __asm__ volatile("rep stosl" : "+D"(start), "+c"(cnt) : "a"(color) : "memory");
}

void bochs_vline(uint32_t x, uint32_t y,uint32_t h, uint32_t color) {
    display_info_t *info = &g_display_driver->info;
    if (x >= info->width) return;
    if (y >= info->height) return;
    if (y + h > info->height) h = info->height - y;
    if (h == 0) return;
    uint32_t *addr = &info->framebuffer[y * info->width + x];
    for (uint32_t r = 0; r < h; r++) {
        *addr = color;
        addr += info->width;
    }
}

void bochs_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color) {
    int32_t dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int32_t dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int32_t sx = (x2 > x1) ? 1 : -1;
    int32_t sy = (y2 > y1) ? 1 : -1;
    int32_t err = dx - dy;
    while (1) {
        bochs_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int32_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 <  dx) { err += dx; y1 += sy; }
    }
}

void bochs_blit(uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, uint32_t w, uint32_t h) {
    display_info_t *info = &g_display_driver->info;
    if (sx >= info->width || sy >= info->height) return;
    if (dx >= info->width || dy >= info->height) return;
    if (sx + w > info->width)  w = info->width - sx;
    if (sy + h > info->height) h = info->height - sy;
    if (dx + w > info->width)  w = info->width - dx;
    if (dy + h > info->height) h = info->height - dy;
    if (w == 0 || h == 0) return;
    uint32_t bpp = info->bpp;
    uint32_t pitch = info->pitch;
    uint8_t *fb = (uint8_t*)info->framebuffer;
    uint32_t line_bytes = w * bpp;
    if (sy < dy) {
        for (uint32_t y = h; y > 0; y--) {
            uint32_t row = y - 1;
            uint8_t *src = fb + (sy + row) * pitch + sx * bpp;
            uint8_t *dst = fb + (dy + row) * pitch + dx * bpp;
            memcpy(dst, src, line_bytes);
        }
    } else {
        for (uint32_t row = 0; row < h; row++) {
            uint8_t *src = fb + (sy + row) * pitch + sx * bpp;
            uint8_t *dst = fb + (dy + row) * pitch + dx * bpp;
            memcpy(dst, src, line_bytes);
        }
    }
}

//Bochs驱动初始化
STATUS bochs_init(display_driver_t *self, gpu_device_t *gpu) {
    //通过 Bochs VBE探测
    vbe_w(VBE_REG_ID, 0xB0C0);
    uint16_t id = vbe_r(VBE_REG_ID);
    if (id < 0xB0C0 || id > 0xB0C5) return 1;//检查是否支持
    gpu->capabilities |= GPU_CAP_BOCHS_VBE;
    gpu->vbe_version = id;
    self->hw = *gpu;//记录硬件信息
    return 0;
}

//Bochs 模式设置
static STATUS bochs_set_mode(display_driver_t *self,uint16_t width, uint16_t height, uint8_t bpp) {
    vbe_w(VBE_REG_ENABLE, 0);//关显示
    io_wait();
    vbe_w(VBE_REG_XRES, width);
    vbe_w(VBE_REG_YRES, height);
    vbe_w(VBE_REG_BPP,  bpp);
    vbe_w(VBE_REG_VIRT_WIDTH, 0);
    vbe_w(VBE_REG_ENABLE, VBE_ENABLE | VBE_LFB_ENABLED | VBE_NOCLEAR);//开显示
    io_wait();
    //取LFB地址
    uint32_t lfb_phys = self->hw.bar[0] & ~0xF;
    if (lfb_phys == 0) lfb_phys = 0xE0000000;
    //更新状态
    self->info.framebuffer = (uint32_t*)(uint64_t)lfb_phys;
    self->info.width  = width;
    self->info.height = height;
    self->info.pitch  = width * (bpp / 8);
    self->info.bpp    = bpp / 8;
    //同步旧全局变量
    FrameBuffer  = self->info.framebuffer;
    ScreenWidth  = width;
    ScreenHeigth = height;
    return 0;
}

//VMware SVGA驱动---------------------------------------------------------------------------------------------
/*DeepSeek-V4-Flash */
//VMware SVGA II 寄存器 (BAR0 MMIO, 32位寄存器偏移)
#define SVGA_REG_ID              0   // 只读, 0x90000001 = SVGA2
#define SVGA_REG_ENABLE          1   // 写1开启
#define SVGA_REG_WIDTH           2
#define SVGA_REG_HEIGHT          3
#define SVGA_REG_BITS_PER_PIXEL  7
#define SVGA_REG_FB_START        9   // LFB 物理地址
#define SVGA_REG_FB_OFFSET       11  // LFB 在 PCI 空间的偏移
#define SVGA_REG_VRAM_SIZE       12  // VRAM 总大小
#define SVGA_REG_FIFO_START      14
#define SVGA_REG_FIFO_SIZE       15
#define SVGA_REG_CONFIG_DONE     25  // 写1完成配置
#define SVGA_REG_SVGA_CAP        (36) // 能力寄存器
#define SVGA_REG_MAX_WIDTH       37
#define SVGA_REG_MAX_HEIGHT      38

//VMware FIFO
#define SVGA_FIFO_MIN            0
#define SVGA_FIFO_MAX            4
#define SVGA_FIFO_NEXT_CMD       8
#define SVGA_FIFO_STOP           12

//VMware 更新命令
#define SVGA_CMD_UPDATE          0

#define VMWARE_REG_BASE  0xFFFFFC0000000000ULL  // （旧）MMIO 寄存器，已废弃，改用 I/O 端口
#define VMWARE_FB_BASE   0xFFFFF80000000000ULL   // 帧缓冲区（映射 BAR1）
#define VMWARE_FIFO_BASE 0xFFFFF40000000000ULL   // FIFO（映射 BAR2）

//VMware SVG 版本魔数
#define SVGA_ID_2           0x90000000
#define SVGA_ID_1           0x90000001

#define SVGA_REG_ID_READ   0x90000001
#define SVGA_REG_ID_READ_2 0x90000002

#define SVGA_CMD_RECT_COPY      3
#define SVGA_REG_SYNC           4

uint16_t g_svga_io_base = 0;//全局存储（set_mode 需要）

uint32_t svga_r(uint32_t reg) {
    if (!g_svga_io_base) return 0;
    outl(g_svga_io_base, reg);//写索引
    return inl(g_svga_io_base + 1);//读值
}
void svga_w(uint32_t reg, uint32_t val) {
    if (!g_svga_io_base) return;
    outl(g_svga_io_base, reg);       // 写索引
    outl(g_svga_io_base + 1, val);   // 写值
}

void vmware_blit(uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, uint32_t w, uint32_t h) {
    display_info_t *info = &g_display_driver->info;
    if (sx >= info->width || sy >= info->height) return;
    if (dx >= info->width || dy >= info->height) return;
    if (sx + w > info->width)  w = info->width - sx;
    if (sy + h > info->height) h = info->height - sy;
    if (dx + w > info->width)  w = info->width - dx;
    if (dy + h > info->height) h = info->height - dy;
    if (w == 0 || h == 0) return;
    uint32_t cmd[7];
    cmd[0] = SVGA_CMD_RECT_COPY;
    cmd[1] = sx;
    cmd[2] = sy;
    cmd[3] = dx;
    cmd[4] = dy;
    cmd[5] = w;
    cmd[6] = h;
    uint32_t *fifo = (uint32_t*)VMWARE_FIFO_BASE;
    uint32_t next_cmd, stop, avail;
    for (;;) {
        next_cmd = fifo[2];
        stop = fifo[3];
        if (next_cmd >= stop) {
            avail = fifo[1] - next_cmd + stop - 16;
        } else {
            avail = stop - next_cmd;
        }
        if (avail >= 32) break;
    }
    for (int i = 0; i < 7; i++) fifo[(next_cmd + i * 4) / 4] = cmd[i];
    next_cmd += 28;
    if (next_cmd >= fifo[1]) next_cmd = 16;
    fifo[2] = next_cmd;
    svga_w(SVGA_REG_SYNC, 1);
}

STATUS vmware_init(display_driver_t *self, gpu_device_t *gpu) {
    if (gpu->vendor_id != VENDOR_VMWARE) return 1;
    //BAR0,I/O 端口
    uint16_t io_base = 0;
    if (gpu->bar[0] & 0x1) {
        io_base = (uint16_t)(gpu->bar[0] & ~0x3);
    }
    uint64_t fb_phys = gpu->bar[1] & ~0xF;//BAR1,帧缓冲区（Framebuffer）
    uint64_t fifo_phys = gpu->bar[2] & ~0xF;//BAR2,FIFO
    if ((io_base == 0) || (fb_phys == 0) || (fifo_phys == 0)) return 1;
    //开启PCI Memory Space & Bus Master
    uint32_t cmd = pci_config_read(gpu->bus, gpu->device, gpu->func, PCI_COMMAND);
    cmd |= 0x6;  // bit1=MemSpace, bit2=BusMaster
    pci_config_write(gpu->bus, gpu->device, gpu->func, PCI_COMMAND, cmd);
    io_wait();
    g_svga_io_base = io_base;
    //设置版本
    svga_w(SVGA_REG_ID, SVGA_ID_2);
    io_wait();
    uint32_t id = svga_r(SVGA_REG_ID);
    if (id < SVGA_ID_2) return 1;
    //记录到 gpu 结构体（供 driver 后续使用）
    gpu->bar[0] = io_base;
    gpu->bar[1] = fb_phys;
    gpu->bar[2] = fifo_phys;
    gpu->capabilities |= GPU_CAP_VMWARE_SVGA;
    gpu->type = GPU_TYPE_VMWARE;
    return 0;
}

STATUS vmware_set_mode(display_driver_t *self, uint16_t width, uint16_t height, uint8_t bpp){
    //从hw中取出各BAR信息
    uint16_t io_base = (uint16_t)self->hw.bar[0];
    uint64_t fb_phys = self->hw.bar[1];
    uint64_t fifo_phys = self->hw.bar[2];
    //写ID确认版本
    svga_w(SVGA_REG_ID, SVGA_ID_2);
    io_wait();
    uint32_t id = svga_r(SVGA_REG_ID);
    //读VRAM大小和最大分辨率
    uint32_t vram = svga_r(SVGA_REG_VRAM_SIZE);
    uint32_t cap  = svga_r(SVGA_REG_SVGA_CAP);
    //设分辨率
    svga_w(SVGA_REG_WIDTH,  width);
    svga_w(SVGA_REG_HEIGHT, height);
    svga_w(SVGA_REG_BITS_PER_PIXEL, bpp);
    io_wait();
    //初始化FIFO
    uint32_t fifo_size = 64 * 1024;  // 64KB
    uint32_t fifo_start = 0;
    svga_w(SVGA_REG_FIFO_START, fifo_start);
    svga_w(SVGA_REG_FIFO_SIZE,  fifo_size);
    io_wait();
    //映射FIFO内存
    for (uint64_t p = 0; p < (fifo_size + 0xFFF) / 0x1000; p++) {
        map_page((void*)(VMWARE_FIFO_BASE + p * 0x1000), (void*)(fifo_phys + p * 0x1000), PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);
    }
    //写 FIFO 头
    uint32_t *fifo = (uint32_t*)VMWARE_FIFO_BASE;
    fifo[SVGA_FIFO_MIN / 4] = 0;
    fifo[SVGA_FIFO_MAX / 4] = fifo_size - 4;
    fifo[SVGA_FIFO_NEXT_CMD / 4] = 16;
    fifo[SVGA_FIFO_STOP / 4] = 16;
    //ENABLE
    svga_w(SVGA_REG_ENABLE, 1);
    io_wait();
    //CONFIG_DONE
    svga_w(SVGA_REG_CONFIG_DONE, 1);
    io_wait();
    //读回验证
    uint32_t rb_w = svga_r(SVGA_REG_WIDTH);
    uint32_t rb_h = svga_r(SVGA_REG_HEIGHT);
    uint32_t rb_bpp = svga_r(SVGA_REG_BITS_PER_PIXEL);
    uint32_t rb_fb = svga_r(SVGA_REG_FB_START);
    //更新状态
    self->info.framebuffer = (uint32_t*)FrameBuffer;
    self->info.width  = width;
    self->info.height = height;
    self->info.pitch  = width * (bpp / 8);
    self->info.bpp    = bpp / 8;
    FrameBuffer  = self->info.framebuffer;
    ScreenWidth  = width;
    ScreenHeigth = height;
    return 0;
}
/*DeepSeek-V4-Flash-END */

//总的探测，绑定
STATUS display_probe(void) {
    gpu_device_t gpus[4];
    int count = pci_scan_gpu(gpus, 4);
    if (count == 0) return 1;
    for (int i = 0; i < count; i++) {
        gpu_device_t *gpu = &gpus[i];
        static display_driver_t drv;
        drv.pixel     = bochs_pixel;
        drv.fill_rect = bochs_fill_rect;
        drv.clear     = bochs_clear;
        drv.hline     = bochs_hline;
        drv.vline     = bochs_vline;
        drv.line      = bochs_line;
        drv.blit = bochs_blit;
        //VMware SVGA探测
        vmware_init(&drv, gpu);
        if ((gpu->vendor_id == VENDOR_VMWARE) && (gpu->capabilities & GPU_CAP_VMWARE_SVGA)) {
            drv.name      = "VMware SVGA II";
            drv.set_mode  = vmware_set_mode;
            drv.hw        = *gpu;
            g_display_driver = &drv;
            return 0;
        }
        //Bochs VBE探测
        vbe_w(VBE_REG_ID, 0xB0C0);
        uint16_t id = vbe_r(VBE_REG_ID);
        if (id < 0xB0C0 || id > 0xB0C5)continue;//不支持，试下一个
        gpu->capabilities |= GPU_CAP_BOCHS_VBE;
        gpu->vbe_version = id;
        drv.name      = "Bochs VBE (BGA)";
        drv.init      = bochs_init;
        drv.set_mode  = bochs_set_mode;
        drv.hw        = *gpu;
        g_display_driver = &drv;
        return 0;
    }
    return 1;
}

//便利函数
display_info_t *display_get_info(void) {
    return g_display_driver ? &g_display_driver->info : NULL;
}