/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

OUTPUT_ARCH("riscv")
ENTRY(_boot)

/* Erbium memory layout */
BOOTROM_BASE = 0x0002008000;
BOOTROM_SIZE = 8K;
SRAM_BASE    = 0x000200C000;
SRAM_SIZE    = 4K;
MSYS_BASE    = 0x0040000000;
MSYS_SIZE    = 256;
MRAM_BASE    = 0x0040000100;
MRAM_SIZE    = 0xFFFF00;

/* Stack configuration — placed at top of MRAM to avoid conflicts with test data */
MAX_THREADS     = 16;
STACK_SIZE      = 4096;
STACK_SIZE_LOG2 = 12;
STACK_MRAM_BASE = 0x0040FF0000;

MEMORY {
    BOOTROM  (rx)  : org = BOOTROM_BASE, len = BOOTROM_SIZE
    SRAM     (rwx) : org = SRAM_BASE,    len = SRAM_SIZE
    MSYS     (r)   : org = MSYS_BASE,    len = MSYS_SIZE
    MRAM     (rwx) : org = MRAM_BASE,    len = MRAM_SIZE
}

/*
 * erbium_mram.ld — variant of erbium.ld where ALL C .text lands in MRAM.
 * Used by large tests (e.g. yolov5s) whose code exceeds the 4KB BOOTROM limit.
 * Boot/init code (bootrom section, .text.init) stays in BOOTROM as usual.
 */
SECTIONS {
    .bootrom : {
        *(bootrom bootrom.*)
        . = ALIGN(8);
        *(.text.init)
        . = ALIGN(8);
        PROVIDE(__code_start = .);   /* placeholder; real code is in MRAM */
        __bootrom_end = .;
    } > BOOTROM

    /* Trap handler at second 4K page of bootrom (0x02009000) */
    .traprom 0x02009000 : {
        *(traprom traprom.*)
    } > BOOTROM

    ASSERT(__bootrom_end <= 0x02009000, ".bootrom overflows into trap handler!")

    /*
     * All C .text and .rodata go to MRAM, placed at 0x40002000 to avoid
     * collision with fixed-address MRAM data structures:
     *   WQ_MRAM_BASE   = 0x40000400 (H0 ring buffers, 8×192 B)
     *   WQ_H1_SHADOW   = 0x40000A00
     *   WQ_H1_MRAM_BASE= 0x40000C00 (H1 ring buffers, 8×192 B)
     *   NN_ARGS_BASE   = 0x40001400 (per-minion conv args, 8×128 B)
     * All reserved regions end at 0x40001800; use 0x40002000 for headroom.
     */
    .mram_text 0x40002000 : {
        . = ALIGN(8);
        *(.text .text.*)
        *(.rodata .rodata.*)
        PROVIDE(__code_end = .);
        *(.mram_text .mram_text.*)
    } > MRAM

    .tpa : {
        . = ALIGN(8);
        PROVIDE(__tpa_proc_user_start = .);
        KEEP(*(.tpa.proc.user))
        PROVIDE(__tpa_proc_user_end = .);

        . = ALIGN(8);
        PROVIDE(__tpa_proc_sys_start = .);
        KEEP(*(.tpa.proc.sys))
        PROVIDE(__tpa_proc_sys_end = .);

        . = ALIGN(8);
        PROVIDE(__tpa_prog_start = .);
        KEEP(*(.tpa.prog))
        PROVIDE(__tpa_prog_end = .);

        . = ALIGN(8);
        PROVIDE(__tpa_boot_start = .);
        KEEP(*(.tpa.boot))
        PROVIDE(__tpa_boot_end = .);
    } > MRAM

    .data : {
        . = ALIGN(8);
        PROVIDE(__data_start = .);
        *(.data .data.*)
        *(.mram_data .mram_data.*)
    } > MRAM

    .sdata : {
        __global_pointer$ = . + 0x800;
        . = ALIGN(8);
        *(.sdata .sdata.*)
        PROVIDE(__data_end = .);
    } > MRAM

    .bss (NOLOAD) : {
        . = ALIGN(8);
        PROVIDE(__bss_start = .);
        *(.bss .bss.*)
    } > MRAM

    .sbss (NOLOAD) : {
        *(.sbss .sbss.*)
        . = ALIGN(8);
        PROVIDE(__bss_end = .);
    } > MRAM

    .stack STACK_MRAM_BASE (NOLOAD) : {
        PROVIDE(__stack_start = .);
        . = . + (MAX_THREADS * STACK_SIZE);
        PROVIDE(__stack_base = .);
    } > MRAM
}
