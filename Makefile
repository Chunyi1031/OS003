#
# OS003 总构建脚本
#
# 默认工具
#	虚拟机:qemu-system-x86_64 
#	分区工具:parted
#
# 2026/7/10 Liu Chunyi
#

.SILENT:

#目录设置
MNT_DIR = ./mnt
BOOT_FILE = Bootloader/build/BOOTx64.EFI
KERNEL_FILE = Kernel/build/kernel
FONTS_FILE = data/font.bin
BOOT_PATH = /EFI/BOOT/BOOTx64.EFI
KERNEL_PATH = /SYS/kernel
FONTS_PATH = /SYS/Fonts10x16.bin
SYSTEM_DISK = ./System.img #虚拟磁盘
SYSTEM_DISK_SIZE = 256     #虚拟磁盘容量（MB）
QEMU_MM_SIZE = 1G

all:
	echo "Compiling bootloader..."
	make -C Bootloader --no-print-directory
	echo "====Boot Build successful===="
	echo "Compiling kernel..."
	make -C Kernel --no-print-directory
	echo "=:)===Build successful===(:="

clean:
	echo "Cleaning bootloader..."
	make -C Bootloader clean --no-print-directory
	echo "Cleaning kernel..."
	make -C Kernel clean --no-print-directory

fix:
	mkdir mnt Bootloader/tmp Bootloader/build Kernel/tmp Kernel/build

disk:
	dd if=/dev/zero of=$(SYSTEM_DISK) bs=1M count=$(SYSTEM_DISK_SIZE)
	# 使用parted创建ESP分区
	sudo parted $(SYSTEM_DISK) mklabel gpt
	sudo parted $(SYSTEM_DISK) mkpart primary fat32 1MiB 201MiB
	sudo parted $(SYSTEM_DISK) set 1 esp on
	sudo parted $(SYSTEM_DISK) print
	echo "Disk:$(SYSTEM_DISK) Size:$(SYSTEM_DISK_SIZE)MB"
	
	echo "Formatting..."
	sudo losetup -Pf $(SYSTEM_DISK)
	LOOP_DEV=$$(sudo losetup -j $(SYSTEM_DISK) | cut -d: -f1); \
	sudo mkfs.fat -F 32 $${LOOP_DEV}p1; \
	sudo losetup -d $$LOOP_DEV
	echo "Done!"
	sudo mount -o loop,offset=1048576 $(SYSTEM_DISK) $(MNT_DIR)
	sudo mkdir -p $(MNT_DIR)/EFI/BOOT $(MNT_DIR)/SYS
	sudo cp $(FONTS_FILE) $(MNT_DIR)$(FONTS_PATH)
	sudo cp data/Test.txt $(MNT_DIR)/Test.txt
	sudo umount $(MNT_DIR)

system:
	sudo mount -o loop,offset=1048576 $(SYSTEM_DISK) $(MNT_DIR)
	echo "Copying boot file..."
	sudo cp $(BOOT_FILE) $(MNT_DIR)$(BOOT_PATH)
	echo "Copying kernel..."
	sudo cp $(KERNEL_FILE) $(MNT_DIR)$(KERNEL_PATH)
	sudo umount $(MNT_DIR)
	echo "Done!"

test:
	qemu-system-x86_64 -m $(QEMU_MM_SIZE) -bios OVMF.fd -hda $(SYSTEM_DISK)

esp:
	mkdir -p ./ESP/EFI/BOOT ./ESP/SYS
	echo "Copying boot file..."
	cp $(BOOT_FILE) ./ESP$(BOOT_PATH)
	echo "Copying kernel..."
	cp $(KERNEL_FILE) ./ESP$(KERNEL_PATH)
	cp $(FONTS_FILE) ./ESP$(FONTS_PATH)
	cp data/Test.txt ./ESP/Test.txt

run:
	make system --no-print-directory
	make test --no-print-directory

vmdk: system
	@printf '# Disk DescriptorFile\nversion=1\nCID=fffffffe\nparentCID=ffffffff\ncreateType="fullDevice"\n\n# Extent description\nRW %d FLAT "%s" 0\n\n# The Disk Data Base\n#DDB\n\nddb.virtualHWVersion = "4"\nddb.geometry.cylinders = "%d"\nddb.geometry.heads = "16"\nddb.geometry.sectors = "63"\nddb.adapterType="ide"\n' \
		$$(( $(SYSTEM_DISK_SIZE) * 1024 * 1024 / 512 )) \
		$(SYSTEM_DISK) \
		$$(( $(SYSTEM_DISK_SIZE) * 1024 * 1024 / 512 / 16 / 63 )) \
		> System.vmdk
	@echo "Done! Use System.vmdk in VirtualBox (keep it next to System.img)."

.PHONY: all clean fix disk vbox vmdk
