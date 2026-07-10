#
# OS003 内核构建脚本
#
# 默认工具
#	CC:x86_64-linux-gnu-gcc
#	LD:x86_64-linux-gnu-ld
#	OBJCOPY:x86_64-linux-gnu-objcopy
#
# 2026/7/10 Liu Chunyi
#

.SILENT:

#GNU-GCC工具链
CC = x86_64-linux-gnu-gcc			#CC编译器
LD = x86_64-linux-gnu-ld			#LD链接器
OBJCOPY = x86_64-linux-gnu-objcopy	#OBJCOPY对象转换器

#目录
TMP_DIR = tmp
BUILD_DIR = build

#文件
MAIN_FILE = 		kernel/kernel.c
PRINT_FILE = 		kernel/Print.c
IDT_FILE = 			kernel/idt.c
PIC_FILE = 			kernel/pic.c
GDT_FILE = 			kernel/gdt.c
TIME_FILE = 		kernel/Time.c
THREAD_FILE = 		kernel/Thread.c
SYSCALL_FILE = 		kernel/Syscall.c
KSTR_FILE = 		lib/kstring.c
DELAY_FILE = 		lib/delay.c
DRAW_FILE = 		lib/Draw.c
IO_FILE = 			lib/io.c
SERIAL_FILE = 		lib/serial.c
TABLE_FILE = 		acpi/tables.c
POWER_FILE = 		acpi/power.c
EMD_FILE = 			mm/EfiMemDesc.c
PG_FILE = 			mm/paging.c
PMM_FILE = 			mm/pmm.c
VMM_FILE = 			mm/vmm.c
BITMAP_FILE = 		mm/bitmap.c
KEYBOARD_FILE = 	drives/Keyboard.c
PCI_FILE = 			drives/pci.c
ATA_FILE = 			drives/ata.c
GPT_FILE = 			drives/gpt.c
DISPLAY_FILE = 		drives/display.c
FAT32_FILE = 		fs/fat32.c
FS_FILE = 			fs/fs.c

MAIN_FILE_OBJ = $(TMP_DIR)/kernel.o
KSTR_FILE_OBJ = $(TMP_DIR)/kstring.o
DELAY_FILE_OBJ = $(TMP_DIR)/delay.o
DRAW_FILE_OBJ = $(TMP_DIR)/Draw.o
PRINT_FILE_OBJ = $(TMP_DIR)/Print.o
IO_FILE_OBJ = $(TMP_DIR)/IO.o
SERIAL_FILE_OBJ = $(TMP_DIR)/serial.o
KEYBOARD_FILE_OBJ = $(TMP_DIR)/Keyboard.o
POWER_FILE_OBJ = $(TMP_DIR)/power.o
TABLE_FILE_OBJ = $(TMP_DIR)/tables.o
EMD_FILE_OBJ = $(TMP_DIR)/EfiMemDesc.o
PMM_FILE_OBJ = $(TMP_DIR)/pmm.o
VMM_FILE_OBJ = $(TMP_DIR)/vmm.o
MEMORY_OBJ = $(TMP_DIR)/Memory.o
IDT_FILE_OBJ = $(TMP_DIR)/idt.o
PIC_FILE_OBJ = $(TMP_DIR)/pic.o
GDT_FILE_OBJ = $(TMP_DIR)/gdt.o
INTERRUPT_OBJ = $(TMP_DIR)/Interrupt.o
TIME_FILE_OBJ = $(TMP_DIR)/Time.o
THREAD_FILE_OBJ = $(TMP_DIR)/Thread.o
BITMAP_FILE_OBJ = $(TMP_DIR)/bitmap.o
PG_FILE_OBJ = $(TMP_DIR)/paging.o
LIB_OBJ = $(TMP_DIR)/lib.o
SYSCALL_FILE_OBJ = $(TMP_DIR)/Syscall.o
PCI_FILE_OBJ = $(TMP_DIR)/pci.o
ATA_FILE_OBJ = $(TMP_DIR)/ata.o
GPT_FILE_OBJ = $(TMP_DIR)/gpt.o
DISPLAY_FILE_OBJ = $(TMP_DIR)/display.o
DRIVES_OBJ = $(TMP_DIR)/Drives.o
FAT32_FILE_OBJ = $(TMP_DIR)/fat32.o
FS_FILE_OBJ = $(TMP_DIR)/fs.o
FILESYS_OBJ = $(TMP_DIR)/FileSystem.o

LINK_FILE = kernel.ld
ELF_FILE = $(TMP_DIR)/kernel.elf
TARGET = $(BUILD_DIR)/kernel

#编译参数
CC_OPT = -c -ffreestanding -fno-stack-protector -mno-red-zone -fno-builtin -I . -I ./include -nostdlib -m64
LD_OPT = -nostdlib -static -T $(LINK_FILE) -e KernelStart --gc-sections --build-id=none
OBJCOPY_OPT = -O binary -R .note -R .comment -S

all: $(TARGET)
clean:
	rm -rf build/* tmp/*

#源文件-OBJ（CC）
$(MAIN_FILE_OBJ):$(MAIN_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(KSTR_FILE_OBJ):$(KSTR_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(DELAY_FILE_OBJ):$(DELAY_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(DRAW_FILE_OBJ):$(DRAW_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(PRINT_FILE_OBJ):$(PRINT_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(IO_FILE_OBJ):$(IO_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -Os $< -o $@
$(TABLE_FILE_OBJ):$(TABLE_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(SERIAL_FILE_OBJ):$(SERIAL_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@
$(KEYBOARD_FILE_OBJ):$(KEYBOARD_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(POWER_FILE_OBJ):$(POWER_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@
$(EMD_FILE_OBJ):$(EMD_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@
$(PG_FILE_OBJ):$(PG_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O1 $< -o $@
$(PMM_FILE_OBJ):$(PMM_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O1 $< -o $@
$(VMM_FILE_OBJ):$(VMM_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O1 $< -o $@
$(IDT_FILE_OBJ):$(IDT_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@
$(PIC_FILE_OBJ):$(PIC_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@
$(GDT_FILE_OBJ):$(GDT_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(TIME_FILE_OBJ):$(TIME_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@
$(THREAD_FILE_OBJ):$(THREAD_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(BITMAP_FILE_OBJ):$(BITMAP_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(SYSCALL_FILE_OBJ):$(SYSCALL_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(PCI_FILE_OBJ):$(PCI_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(ATA_FILE_OBJ):$(ATA_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O0 $< -o $@
$(GPT_FILE_OBJ):$(GPT_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@
$(DISPLAY_FILE_OBJ):$(DISPLAY_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@
$(FAT32_FILE_OBJ):$(FAT32_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@
$(FS_FILE_OBJ):$(FS_FILE)
	echo "CC $@"
	$(CC) $(CC_OPT) -O2 $< -o $@

#多OBJ-OBJ（LD)
$(LIB_OBJ):$(IO_FILE_OBJ) $(KSTR_FILE_OBJ) $(DELAY_FILE_OBJ) $(SERIAL_FILE_OBJ)
	echo "LD $@"
	$(LD) -r $^ -o $@
$(INTERRUPT_OBJ):$(GDT_FILE_OBJ) $(IDT_FILE_OBJ) $(PIC_FILE_OBJ)
	echo "LD $@"
	$(LD) -r $^ -o $@
$(MEMORY_OBJ):$(EMD_FILE_OBJ) $(BITMAP_FILE_OBJ) $(PMM_FILE_OBJ) $(PG_FILE_OBJ) $(VMM_FILE_OBJ)
	echo "LD $@"
	$(LD) -r $^ -o $@
$(DRIVES_OBJ):$(KEYBOARD_FILE_OBJ) $(PCI_FILE_OBJ) $(ATA_FILE_OBJ) $(GPT_FILE_OBJ) $(DISPLAY_FILE_OBJ)
	echo "LD $@"
	$(LD) -r $^ -o $@
$(FILESYS_OBJ):$(FAT32_FILE_OBJ) $(FS_FILE_OBJ)
	echo "LD $@"
	$(LD) -r $^ -o $@

#所有OBJ-ELF（LD）
$(ELF_FILE):$(MAIN_FILE_OBJ) $(LIB_OBJ) $(TABLE_FILE_OBJ) $(DRAW_FILE_OBJ) $(PRINT_FILE_OBJ) $(TIME_FILE_OBJ) $(INTERRUPT_OBJ) $(MEMORY_OBJ) $(DRIVES_OBJ) $(THREAD_FILE_OBJ) $(POWER_FILE_OBJ) $(SYSCALL_FILE_OBJ) $(FILESYS_OBJ)
	echo "LD $@"
	$(LD) $(LD_OPT) $^ -o $@

#ELF-BIN-kernel(OBJCOPY+CAT)
$(TARGET):$(ELF_FILE)
	echo "OBJCOPY $@"
	$(OBJCOPY) $(OBJCOPY_OPT) $< $(TMP_DIR)/kernel.bin
	cat head.bin $(TMP_DIR)/kernel.bin > $@
	rm $(TMP_DIR)/kernel.bin
	echo "Build successful"
	echo "File:$@"

.PHONY:all clean
