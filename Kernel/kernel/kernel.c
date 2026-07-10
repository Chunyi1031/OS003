/**
 * OS003 内核入口点
 * 入口点:KernelStart
 * 2026/7/10 Liu Chunyi
 */

#include <klib.h>
#include <Draw.h>
#include <Print.h>
#include <ACPI.h>
#include <Time.h>
#include <Interrupt.h>
#include <Keyboard.h>
#include <Power.h>
#include <Memory.h>
#include <Thread.h>
#include <Syscall.h>
#include <drives/pci.h>
#include <drives/ata.h>
#include <drives/gpt.h>
#include <drives/display.h>
#include <fs/fs.h>

#define DefaultConsoleStyle (ConsoleStyle){COLOR_WHITE,COLOR_BLACK}
#define CmdBufferSize 64

uint64_t SYSTEM_CPU_Frequency = 0;
VMM_T* Current_VMM_Desc = NULL;
volatile char SYSTEM_KeyboardBuffer[KEYBOARD_BUFFER_SIZE];
thread_t *SYSTEM_KernelThread = NULL;
UEFI_MEMORY_MAP* SYSTEM_MemoryMap = NULL;
BOOT_SHARE* SYSTEM_BootShare = NULL;
uint16_t UEFI_CS = 0;
uint16_t UEFI_DS = 0;
pml4_t* UEFI_PML4 = NULL;
pml4_t* KernelPML4 = NULL;
ata_controller_info_t gATAInfo;

_Bool LoadBootShare(BOOT_SHARE* BootShare);//加载引导数据
_Bool InitSystem();//初始化系统
void InitConsole(ConsoleStyle style);//初始化控制台
int execute(char* cmd);//执行命令

void cmd_clear();//clear
void cmd_clock();//clock

//链接脚本导出的 BSS 符号
extern uint64_t __bss_start[];
extern uint64_t __bss_end[];

//内核入口点
void KernelStart(BOOT_SHARE* BootShare){
    //清零BSS段
    uint64_t* bss = __bss_start;
    while (bss < __bss_end) {
        *bss++ = 0;
    }
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 &= ~((1ULL << 20) | (1ULL << 21));//清除SMEP和SMAP
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
    if(!LoadBootShare(BootShare))SYSTEM_STOP();//加载引导数据
    if(!InitSystem())SYSTEM_STOP();//初始化系统
    InitConsole(DefaultConsoleStyle);//初始化控制台
    //OS003 Kernel Shell
    char key = 0;
    char CmdBuffer[CmdBufferSize];
    int CharInCmd = 0;
    while (1){
        while (1){
            if(PrintLine*18 > (ScreenHeigth - 30)){
                cmd_clear();
                print("Shell>",0xFFAAAAAA);
                break;
            }
            key = GetKey();
            if(key == 0)break;
            if((key == '\n') || (CharInCmd >= (CmdBufferSize-1))){
                printf("\n");
                CmdBuffer[CharInCmd] = '\0';
                if(CharInCmd > 0)execute(CmdBuffer);//执行命令
                print("Shell>",0xFFAAAAAA);
                //清空命令缓冲区
                CharInCmd = 0;
                CmdBuffer[0] = '\0';
                break;
            }
            //处理退格符
            if((key == '\b')&&(CharInCmd > 0)){
                if(PrintRow > 0){
                    PrintRow --;//光标回退
                    CharInCmd --;//缓冲区“光标”回退
                    fillRect(PrintRow*10,PrintLine*18,12,16,CurrentConsoleStyle.BgColor);//清除字符
                    CmdBuffer[CharInCmd] = 0;//清除缓冲区
                }
                break;
            }else if((key == '\b')&&(CharInCmd == 0)){
                break;
            }
            CmdBuffer[CharInCmd] = key;
            printf("%c",key);
            CharInCmd ++;
        }
    }
    SYSTEM_STOP();

}

//测试内核线程
thread_t* test_thread;
void thread_test(){  
    while (1){
        fillRect(400,400,5,5,COLOR_RED);
        ThreadSleepSecond(1);
        fillRect(400,400,5,5,COLOR_GREEN);
        ThreadSleepSecond(1);
        fillRect(400,400,5,5,COLOR_BLUE);
        ThreadSleepSecond(1);
    }
}
//测试独立虚拟内存空间线程
void thread_uvmt(){
    printf("This is a thread that uses virtual memory.\n");
    int a = 100;
    uint64_t b = 1234;
    printf("a = %d b = %d\na at %p b at %p\n",a,b,&a,&b);
    printf("%p\n",Current_VMM_Desc->Descriptor->Address);
    ThreadSleepSecond(5);
}
//测试用户线程
void thread_usertest(){
    char s[14] = "Hello World!\n";
    for(int i=0;i<14;i ++){syscall(SYS_putchar,s[i]);}
    syscall(SYS_sleep,2000);
    char s1[24] = "This is a ring3 thread\n";
    for(int i=0;i<24;i ++){syscall(SYS_putchar,s1[i]);}
    syscall(SYS_sleep,1000);
    syscall(SYS_exit,0);
}

void thread_fillrect(){
    fillRect(ScreenWidth/2,0,ScreenWidth/2,ScreenHeigth,COLOR_YELLOW);
}

int execute(char* cmd){
    //清屏
    if(strcmp(cmd,"clear") == 0){
        cmd_clear();
    //系统状态
    }else if(strcmp(cmd,"status") == 0){
        printf("Operating System:   %s\n",OS_NAME);
        printf("CPU Frequency:      %u\n",SYSTEM_CPU_Frequency);
        printf("Screen Size:        %dx%d\n",ScreenWidth,ScreenHeigth);
        printf("Fonts Addr:         %p\n",font_data);
        printf("ACPI RSDP Addr:     %p\n",SYSTEM_RSDP_ADDR);
        printf("Total Memory:       %dMB\n",GetMemoryTotalSize()/1024);
        printf("PML4:               %p\n",get_cr3());
    //关机
    }else if(strcmp(cmd,"shutdown") == 0){
        delay_ms(100);
        ClearScreen();
        print("[POWER]Shutting down...\n",COLOR_CYAN);
        delay_seconds(1);
        SYSTEM_SHUTDOWN();
    //重启
    }else if(strcmp(cmd,"reboot") == 0){
        delay_ms(100);
        ClearScreen();
        print("[POWER]Reboot...\n",COLOR_CYAN);
        delay_seconds(1);
        SYSTEM_REBOOT();
    //时间
    }else if(strcmp(cmd,"time") == 0){
        struct rtc_time time;
        get_rtc_time(&time);//获取时间
        printf("20%d/%d/%d\n%d:%d:%d\n",time.year,time.month,time.day,time.hour,time.minute,time.second);//显示时间
    //时钟
    }else if(strcmp(cmd,"clock") == 0){
        cmd_clock();
    //第6个内存描述符信息
    }else if(strcmp(cmd,"UEFImd") == 0){
        UEFI_MEMORY_DESCRIPTOR* MemoryDescriptor = AnalysisMemoryMap(0);//获取描述符地址
        if(!MemoryDescriptor){
            out_error("AnalysisMemoryDescriptor failed\n");//如果无效则打印错误信息
        }else{
            PrintMemoryDescriptor(MemoryDescriptor);//否则打印描述符信息
        }
        printf("Total Memory: %dMB\n",GetMemoryTotalSize()/1024);
    //物理内存数据
    }else if(strcmp(cmd,"pmm") == 0){
        printf("%p\n",MemDescAddr);
        uint32_t maxuednum = SYSTEM_MemoryMap->MapSize / SYSTEM_MemoryMap->DescriptorSize;//计算描述符数量
        printf("%d  %d\n",maxuednum,MemDescNum);
        uint64_t size = 0;
        for(int a = 0;a < MemDescNum;a ++){
            size += MemDescAddr[a].PageSize*4;
            printf("%d: Addr:%p  Size:%dKB %s\n",a,MemDescAddr[a].PhysicalStart,MemDescAddr[a].PageSize*4,MemDescAddr[a].Type == OS_AVAILABLE_MEMORY?"AVAILABLE":"UNAVAILABLE");
        }
        printf("Total %dMB\n",size/1024);
    //分配物理内存测试
    }else if(strcmp(cmd,"acpmm") == 0){
        //首次分配
        void* a = Pmm_Malloc(2);
        void* b = Pmm_Malloc(4);
        printf("%p %p\n",a,b);
        Pmm_Free(b,4);//释放
        b = Pmm_Malloc(4);//再次分配
        printf("%p %p\n",a,b);//如果与上次相同，则成功
        //全部释放
        Pmm_Free(a,2);
        Pmm_Free(b,4);
    //启动测试线程
    }else if(strcmp(cmd,"StartTest") == 0){
        test_thread = CreateKernelThread(thread_test,1024);
    //结束测试线程
    }else if(strcmp(cmd,"KillTest") == 0){
        KillThread(test_thread);
    //列出当前线程
    }else if(strcmp(cmd,"ThreadLs") == 0){
        PrintRunningThreads();
    //虚拟内存数据 
    }else if(strcmp(cmd,"vmm") == 0){
        printf("VMM:\n");
        printf(" table at %p\n",(uintptr_t)Current_VMM_Desc);
        printf(" PML4 at %p\n",(uintptr_t)Current_VMM_Desc->Pml4);
        printf(" desc at %p\n",(uintptr_t)Current_VMM_Desc->Descriptor);
        printf("VMM Desc:\n");
        printf(" VMM Descriptor at %p\n",Current_VMM_Desc->Descriptor);
        printf(" Start Addr:%p\n",Current_VMM_Desc->Descriptor->Address);
        printf(" Pagesize:%dB\n",Current_VMM_Desc->Descriptor->Bytes);
        printf("VMM_ERRORCODE:      %d\n",VMM_ERRORCODE);
        printf("Current VMM:        %p\n",current_thread->vmm);
        printf("Allocated %d times\n",Current_VMM_Desc->AllocRecordNum);
    //分配虚拟内存测试
    }else if(strcmp(cmd,"acvmm") == 0){
        char* virt_addr = Vmm_Malloc(1024);
        virt_addr[0] = 'a';
        printf("%c\n",virt_addr[0]);
        printf("%p\n",virt_addr);
        Vmm_Free(virt_addr,1024);
    //使用虚拟内存的线程测试
    }else if(strcmp(cmd,"uvmth") == 0){
        VMM_T* vmm = Init_Virtual_Memory_Manager(PTE_PRESENT | PTE_WRITABLE);
        VMM_T* old_vmm = (VMM_T*)Switch_Virtual_Mamanager(vmm);
        void* stack = Vmm_Malloc(4096);
        Switch_Virtual_Mamanager(old_vmm);
        printf("Stack(v) at %p,Stack(p) at %p\n",stack,virt_to_phys_on(vmm->Pml4,stack));
        thread_t* thread = create_thread(thread_uvmt,(void*)stack,4096,true,vmm);
        delay_seconds(1);
    //用户线程测试
    }else if(strcmp(cmd,"uvltest") == 0){
        thread_t* r3_test = CreateUserThread(thread_usertest, 4096);
        delay_ms(2100);
        printf("[KERNEL]ID:%d,Stack at %p\n",r3_test->id,r3_test->stack);
        delay_ms(1000);
    //扫描硬盘
    }else if(strcmp(cmd,"disk") == 0){
        ATAPrintInfo(&gATAInfo);
    //寻找ESP分区
    }else if(strcmp(cmd,"findpart") == 0){
        ata_disk_t disk = FindFirstATADisk();
        // ESP分区类型GUID
        const uint8_t esp_guid[16] = {
            0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
            0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
        };
        uint8_t buffer[128];
        STATUS status = FindPartition(&disk.channel, disk.is_master, esp_guid, (gpt_partition_entry_t*)buffer);
        gpt_partition_entry_t* partition = (gpt_partition_entry_t*)buffer;
        if(status == STATUS_SUCCESS){
            printf("Found ESP parttion:\n");
            printf("Start:%d End:%d\nPartition name:",partition->starting_lba,partition->ending_lba);
            // 将UTF-16LE转换为UTF-8
            for (int i = 0; i < 36 && partition->partition_name[i] != 0; i++) {
                printf("%c",(char)partition->partition_name[i]);
            }
            printf("\n");
            printf("Size:%d\n",(partition->ending_lba-partition->starting_lba));
        }else{
            printf("No ESP partition found:%d\n",status);
        }
    //打印FAT32分区信息
    }else if(strcmp(cmd,"fat32info") == 0){
        ata_disk_t disk = FindFirstATADisk();
        gpt_partition_entry_t partition;
        STATUS status = FindPartition(&disk.channel, disk.is_master, ESP_GUID, &partition);
        if(status == STATUS_SUCCESS){
            fat32_info_t Info;
            STATUS status1 = FAT32_GetInfo(&disk,&partition,&Info);
            if(status1 == STATUS_SUCCESS){
                printf("root_cluster:%d\n",Info.root_cluster);
                printf("FAT LBA:%d\n",Info.fat_region_lba);
                printf("Data LBA:%d\n",Info.data_region_lba);
                printf("Total Size:%dKB\n",Info.total_sectors*Info.bytes_per_sector/1024);
                printf("Used Size:%dKB\n",(Info.total_sectors-Info.free_clusters*Info.sectors_per_cluster)*512/1024);
                printf("VolumeLabel:%s\n",Info.volume_label);
                printf("Volume ID:%X\n",Info.volume_id);
            }else{
                printf("Get fat32 info failed:%d\n",status1);
            }
        }else{
            printf("Find partition failed:%d\n",status);
        }
    //读取文件（/Test.txt）
    }else if(strcmp(cmd,"readfile") == 0){
        file_t file = OpenFile("/Test.txt",FILE_READ);
        fat32_dir_entry_t entry = *file.entry;
        printf("Found file %s  Size:%d Bytes\n",entry.name,entry.file_size);
        printf("Create:%d/%d/%d %d:%d:%d\n",get_bits(entry.creation_date,9,7)+1980,get_bits(entry.creation_date,5,4),get_bits(entry.creation_date,0,5),
                                          get_bits(entry.creation_time,11,5),get_bits(entry.creation_time,5,6),get_bits(entry.creation_time,0,5)*2);
        //读取文件
        char *buffer = Pmm_Malloc(1);
        ReadFile(&file,buffer);
        printf(buffer);
        Pmm_Free(buffer,1);
        CloseFile(&file);
    //覆盖写入文件并还原（/Test.txt）
    }else if(strcmp(cmd,"writefile") == 0){
        file_t file = OpenFile("/Test.txt",FILE_WRITE | FILE_READ);
        //读取文件
        uint8_t* buffer = Pmm_Malloc(1);
        ReadFile(&file,buffer);
        printf("1: %s\n",buffer);
        printf("File Size:%dBytes\n",file.entry->file_size);
        //修改文件
        WriteFile(&file,"ABCDE",5);
        //再次读取
        uint8_t* buffer_ = Pmm_Malloc(1);
        ReadFile(&file,buffer_);
        printf("2: %s\n",buffer_);
        printf("File Size:%dBytes\n",file.entry->file_size);
        Pmm_Free(buffer_,1);
        //还原
        WriteFile(&file,buffer,strlen(buffer));
        Pmm_Free(buffer,1);//回收
        CloseFile(&file);
    //追加写入文件（/Test.txt）
    }else if(strcmp(cmd,"appendfile") == 0){
        file_t file = OpenFile("/Test.txt",FILE_WRITE | FILE_READ);
        //读取文件
        uint8_t* buffer = Pmm_Malloc(1);
        ReadFile(&file,buffer);
        printf("1: %s\n",buffer);
        printf("File Size:%dBytes\n",file.entry->file_size);
        //修改文件
        AppendFile(&file,"ABC",3);
        //再次读取
        ReadFile(&file,buffer);
        printf("2: %s\n",buffer);
        printf("File Size:%dBytes\n",file.entry->file_size);
        Pmm_Free(buffer,1);
        CloseFile(&file);
    //列出根目录
    }else if(strcmp(cmd,"ls") == 0){
        int count;
        int dir_pages;
        fat32_list_entry_t* dir = ListDirectory("/",&count,&dir_pages);
        //打印目录
        for(int i = 0;i < count;i ++){
            int year = get_bits(dir[i].entry.last_write_date,9,7)+1980;
            int month = get_bits(dir[i].entry.last_write_date,5,4);
            int date = get_bits(dir[i].entry.last_write_date,0,5);
            int hour = get_bits(dir[i].entry.last_write_time,11,5);
            int minute = get_bits(dir[i].entry.last_write_time,5,6);
            if(dir[i].entry.attr & FAT32_ATTR_DIRECTORY){
                printf("[DIR]  %s           %d/%d/%d %d:%d\n", dir[i].display_name,year,month,date,hour,minute);
            }else{
                printf("[FILE] %s  (%d KB)  %d/%d/%d %d:%d\n",dir[i].display_name,(dir[i].entry.file_size + 1023) / 1024,year,month,date,hour,minute);
            }
        }
        if(dir_pages < 1) dir_pages = 1;
        Pmm_Free(dir, dir_pages);
    //修改文件名并还原（/Test.txt->ABC.TXT->/Test.txt）
    }else if(strcmp(cmd,"rename") == 0){
        execute("ls");
        //首次修改文件名
        RenameFile("/Test.txt","ABC.TXT");
        execute("ls");
        //还原文件名
        RenameFile("/ABC.TXT","Test.txt");
    //创建一个文件并删除（/NewFile.txt"）
    }else if(strcmp(cmd,"touch") == 0){
        STATUS Status = CreateFile("/NewFile.txt");
        if(Status != STATUS_SUCCESS){
            printf("Create failed:%d\n",Status);
            return 1;
        }
        execute("ls");
        Status = DeleteFile("/NewFile.txt");
        if(Status != STATUS_SUCCESS)printf("Remove failed:%d\n",Status);
    //创建一个目录并删除（/NewDir）
    }else if(strcmp(cmd,"mkdir") == 0){
        STATUS Status = CreateDir("/NewDir");
        if(Status != STATUS_SUCCESS){
            printf("Create failed:%d\n",Status);
            return 1;
        }
        execute("ls");
        Status = RemoveDir("/NewDir");
        if(Status != STATUS_SUCCESS)printf("Remove failed:%d\n",Status);
    //显示显卡星系
    }else if(strcmp(cmd,"gpu") == 0){
        gpu_device_t gpus[4];
        int count = pci_scan_gpu(gpus, 4);
        if(count == 0){ printf("No GPU found\n"); return 1; }
        for(int i = 0; i < count; i++){
            printf("GPU %d: %X:%X  Bus%d Dev%d Func%d\n",i, gpus[i].vendor_id, gpus[i].device_id,gpus[i].bus, gpus[i].device, gpus[i].func);
            printf("  BAR0: %X  IRQ: %d\n",gpus[i].bar[0] & ~0xF, gpus[i].irq_line);
        }
        printf("Active driver: %s\n",g_display_driver ? g_display_driver->name : "none");
        printf("Current mode: %dx%d@%dbpp\n",display_get_info()->width, display_get_info()->height, display_get_info()->bpp * 8);
    //设置显卡模式（切换分辨率为1920x1080）
    }else if(strcmp(cmd,"gpumode") == 0){
        if(!g_display_driver || !g_display_driver->set_mode){
            printf("No GPU driver\n"); 
            return 1;
        }
        int idx = 0;
        uint16_t w = 1920, h = 1080;
        idx = (idx + 1) % 4;
        printf("Switch to %dx%d...\n", w, h);
        if(g_display_driver->set_mode(g_display_driver, w, h, 32) == 0){
            ClearScreen();
            printf("Now %dx%d via %s\n", w, h, g_display_driver->name);
        }else{
            printf("Mode set failed\n");
        }
    //位块传输测试
    }else if(strcmp(cmd,"blit") == 0){
        display_fill_rect(20,20,60,60,COLOR_CYAN);
        display_fill_rect(80,40,40,80,COLOR_YELLOW);
        display_line(20,20,120,120,COLOR_RED);
        display_blit(20, 20, 250,20,100,100);
    }else{
        out_waring("Command not found\n");
    }
    return 0;
}

_Bool LoadBootShare(BOOT_SHARE* BootShare){
    //检查有效性
    if(!BootShare)return false;
    if(!BootShare->MemoryMap || !BootShare->FONTADDR || !BootShare->RSDP || !BootShare->FONTADDR) return false;
    SYSTEM_BootShare = BootShare;
    FrameBuffer = (uint32_t*)BootShare->ScreenData->FrameBuffer;//设置缓冲区
    SYSTEM_CPU_Frequency = BootShare->CPUF;//CPU基准频率
    //屏幕分辨率
    ScreenHeigth = BootShare->ScreenData->SH;
    ScreenWidth = BootShare->ScreenData->SW;
    font_data = (uint8_t*)(uint64_t)BootShare->FONTADDR;//字体
    SYSTEM_MemoryMap = BootShare->MemoryMap;//内存描述符
    return true;
}

_Bool InitSystem(){
    PrintLine = 0;
    PrintRow = 0;
    __asm__ volatile("mov %%cs, %0" : "=r"(UEFI_CS));
    __asm__ volatile("mov %%ds, %0" : "=r"(UEFI_DS));
    UEFI_PML4 = (pml4_t*)get_cr3();
    CurrentConsoleStyle = (ConsoleStyle){COLOR_WHITE, COLOR_BLACK};
    //初始化ACPI
    if(init_ACPI((RSDP_DESCRIPTOR*)SYSTEM_BootShare->RSDP)){
        out_ok("ACPI initialization successful\n",0);
    }else{
        out_error("ACPI initialization failed\n");
        return false;
    }
    serial_init(SERIAL_COM1_PORT);//初始化串口
    gdt_init();
    init_syscall_msrs();
    UEFI_CS = KERNEL_CS;
    UEFI_DS = KERNEL_DS;
    //初始化中断
    Init_Interrupt();
    out_ok("INT initialization successful\n",0);
    //初始化物理内存管理
    if(Init_Physical_Memory_Manager() != 0){
        out_error("Physical Memory Manager initialization failed\n");
        return false;
    }
    //初始化内核虚拟内存
    VMM_T* table = Init_Virtual_Memory_Manager(PTE_PRESENT | PTE_WRITABLE | PTE_GLOBAL);
    Switch_Virtual_Mamanager(table);//切换虚拟内存管理
    KernelPML4 = (pml4_t*)get_cr3();
    init_multitasking();
    //扫描硬盘
    pci_device_t storage_devs[8];
    int dev_count = pci_find_storage_controllers(storage_devs, 8);
    for (int i = 0; i < dev_count; i++) {
        if (storage_devs[i].class_code == PCI_CLASS_MASS_STORAGE && storage_devs[i].subclass == PCI_SUBCLASS_IDE) {
            gATAInfo.controller_found = true;
            gATAInfo.vendor_id = storage_devs[i].vendor_id;
            gATAInfo.device_id = storage_devs[i].device_id;
            ata_scan_all(&gATAInfo);
            break;
        }
    }
    //初始化文件系统
    if(InitFileSystem(ESP_GUID) == 0)out_ok("FS initialization successful\n",0);
    //探测显卡,绑定驱动
    if(display_probe() == 0 && g_display_driver != NULL){
        out_ok("Display driver: ",0);
        printf("%s\n", g_display_driver->name);
        //自动设模式
        g_display_driver->set_mode(g_display_driver, 1280, 720, 32);
    }else{
        out_waring("No GPU driver found, using UEFI FB\n");
    }
    //测量CPU频率
    uint64_t cpuf = Get_CPU_Frequency();
    if(cpuf)SYSTEM_CPU_Frequency = cpuf;
    memset((void*)&SYSTEM_KeyboardBuffer,0,sizeof(SYSTEM_KeyboardBuffer));
    serial_putstr(SERIAL_COM1_PORT,"[OK]OS003 Kernel is running\n");
    return true;
}

void InitConsole(ConsoleStyle style){
    CurrentConsoleStyle = style;
    ClearScreen();
    out_ok("OS003 Kernel is running:\n",0);
    struct rtc_time time;
    get_rtc_time(&time);
    printf("  20%d/%d/%d\n  %d:%d:%d\n",time.year,time.month,time.day,time.hour,time.minute,time.second);
    printf("OS003 Shell\n");
    print("Shell>",0xFFAAAAAA);
}

void SYSTEM_STOP(){
    while (1){
        asm("hlt");//降低CPU使用率
    }
}

void UpdateCursor(uint32_t color){
    for(int i = 0;i < 16;i ++){
        DrawPoint(PrintRow * 10,PrintLine * 18+i,color);
    }
}

void cmd_clear(){
    ClearScreen();
}

void cmd_clock(){
    while(1){
        fillRect(0,0,ScreenWidth,ScreenHeigth,COLOR_GREY);//清屏
        //设置光标
        PrintLine = 2;
        PrintRow = 2;
        struct rtc_time time;
        get_rtc_time(&time);//获取时间
        printf("20%d/%d/%d\n%d:%d:%d\n",time.year,time.month,time.day,time.hour,time.minute,time.second);//显示
        //如果按下E键则推出
        if((GetKey_NoBlock() == 'e')||(GetKey_NoBlock() == 'E')){
            break;
        }
        delay_ms(600);
    }
}
