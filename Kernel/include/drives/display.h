/**
 * OS003 硬件驱动-显卡驱动库
 * Bochs VBE    VMware SVGA 3D
 * 2026/7/10 Liu Chunyi
 */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <klib.h>

//GPU能力位
#define GPU_CAP_LFB         1
#define GPU_CAP_BOCHS_VBE   2
#define GPU_CAP_VMWARE_SVGA 4

//显卡硬件信息（PCI 读出来的）
typedef struct gpu_device {
    uint8_t  bus, device, func;
    uint16_t vendor_id, device_id;
    uint8_t  class_code, subclass, prog_if, revision;
    uint32_t bar[6];
    uint8_t  irq_line;
    uint8_t  type;              // GPU_TYPE_*
    uint32_t capabilities;      // GPU_CAP_*
    uint16_t vbe_version;       // Bochs VBE 版本
} gpu_device_t;

//当前显示状态
typedef struct display_info {
    uint32_t *framebuffer;      // LFB 地址（页面已映射）
    uint16_t  width, height;    // 当前分辨率
    uint16_t  pitch;            // 每行字节数
    uint8_t   bpp;              // 每像素字节数
} display_info_t;

//驱动操作表
struct display_driver;

typedef struct display_driver {
    const char *name; //驱动名称
    STATUS (*init)(struct display_driver *self,gpu_device_t *gpu);//探测+初始化
    STATUS (*set_mode)(struct display_driver *self,uint16_t w, uint16_t h,uint8_t bpp);//设分辨率
    void   (*pixel)(uint32_t x, uint32_t y,uint32_t color);//画点
    void   (*fill_rect)(uint32_t x, uint32_t y,uint32_t w, uint32_t h,uint32_t color);//填矩形
    void   (*clear)(uint32_t color);//清屏
    void   (*hline)(uint32_t x, uint32_t y,uint32_t w, uint32_t color);//水平线
    void   (*vline)(uint32_t x, uint32_t y,uint32_t h, uint32_t color);//垂直线
    void   (*line)(int32_t x1, int32_t y1,int32_t x2, int32_t y2, uint32_t color);//斜线
    gpu_device_t    hw;//所驱动的显卡
    void   (*blit)(uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, uint32_t w, uint32_t h);//位块传输
    display_info_t  info;//当前显示状态
} display_driver_t;

extern display_driver_t *g_display_driver;//当前活动的驱动

/**
 * 扫描 + 探测 + 绑定驱动
 * 自动找到显卡、识别类型、挂载对应的 ops
 * @return 0=成功
 */
STATUS display_probe(void);

display_info_t *display_get_info(void);//获取当前显示信息

//通过当前驱动调操作
#define display_set_mode(w,h,bpp)  g_display_driver->set_mode(g_display_driver,w,h,bpp)
#define display_pixel(x,y,c)       g_display_driver->pixel(x,y,c)
#define display_fill_rect(x,y,w,h,c) g_display_driver->fill_rect(x,y,w,h,c)
#define display_clear(c)           g_display_driver->clear(c)
#define display_hline(x,y,w,c)     g_display_driver->hline(x,y,w,c)
#define display_vline(x,y,h,c)     g_display_driver->vline(x,y,h,c)
#define display_line(x1,y1,x2,y2,c) g_display_driver->line(x1,y1,x2,y2,c)
#define display_blit(sx,sy,dx,dy,w,h) g_display_driver->blit(sx,sy,dx,dy,w,h)

#endif