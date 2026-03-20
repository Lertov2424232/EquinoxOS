OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(kmain)

PHDRS {
    /* PT_LOAD - сегмент, который нужно загрузить в память */
    text    PT_LOAD FLAGS(5);   /* Read + Execute */
    rodata  PT_LOAD FLAGS(4);   /* Read Only */
    data    PT_LOAD FLAGS(6);   /* Read + Write */
}

SECTIONS {
    /* Базовый адрес ядра */
    . = 0xffffffff80100000;

    /* ---- СЕГМЕНТ КОДА ---- */
    .text : {
        *(.text .text.*)
        
        . = ALIGN(8);
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } :text

    /* Переходим на следующую страницу (4KB), чтобы не смешивать права доступа */
    . = ALIGN(0x1000);

    /* ---- СЕГМЕНТ КОНСТАНТ ---- */
    .rodata : {
        *(.rodata .rodata.*)

        /* Маркеры запросов Limine должны быть здесь */
        KEEP(*(.limine_requests_start_marker))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end_marker))
    } :rodata

    . = ALIGN(0x1000);

    /* ---- СЕГМЕНТ ДАННЫХ ---- */
    .data : {
        *(.data .data.*)
        /* Твой огромный буфер тоже попадет сюда, если он инициализирован */
        *(.doom_buffer)
    } :data

    /* BSS обычно живет в том же сегменте данных (R+W) */
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /* Убираем ненужные секции */
    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }
}