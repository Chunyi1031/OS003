/*
    OS003 内核链接脚本
  2026/7/10 Liu Chunyi
*/

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x100000;
    
    . = ALIGN(4K);
    .text : {
        *(.text.start)
        *(.text)
        *(.gnu.linkonce.t.*)
        . = ALIGN(16);
    }
    
    . = ALIGN(4K);
    .rodata : {
        *(.rodata)
    }
    
    . = ALIGN(4K);
    .data : {
        *(.data)
    }
    
    . = ALIGN(4K);
    .bss : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }

    _end = .;
    
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
    }
}