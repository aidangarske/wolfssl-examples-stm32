/*
 * Linker script for STM32U385RG app loaded by wolfBoot.
 *
 * wolfBoot lives at 0x08000000 (64 KB).
 * BOOT partition starts at 0x08010000.
 * wolfBoot image header is 256 bytes (IMAGE_HEADER_SIZE).
 * App vectors start at 0x08010100.
 *
 * Flash layout (dual-bank, 4 KB pages):
 *   0x08000000 – 0x0800FFFF  wolfBoot         (64 KB, Bank 1)
 *   0x08010000 – 0x0807FFFF  BOOT partition   (448 KB, Bank 1)
 *     0x08010000 – 0x080100FF  wolfBoot image header (256 B)
 *     0x08010100 – ...         App (this binary)
 *   0x08080000 – 0x080EFFFF  UPDATE partition (448 KB, Bank 2)
 *   0x080F0000 – 0x080F0FFF  SWAP sector      (4 KB, Bank 2)
 */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = 0x1000;  /* 4 KB heap */
_Min_Stack_Size = 0x4000;  /* 16 KB stack - P-384 ECC needs ~10 KB */

MEMORY
{
  /* App vectors land 512 B past BOOT partition start: wolfBoot's
   * IMAGE_HEADER_SIZE is 512 bytes for ECC384 + SHA384 (the size of the
   * signed manifest). BOOT @ 0x08010000 + 0x200 manifest = 0x08010200. */
  FLASH (rx)  : ORIGIN = 0x08010200, LENGTH = 0x6FE00
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 192K
  RAM2  (xrw) : ORIGIN = 0x20030000, LENGTH = 64K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab (READONLY) :
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM (READONLY) :
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array (READONLY) :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array (READONLY) :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array (READONLY) :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
