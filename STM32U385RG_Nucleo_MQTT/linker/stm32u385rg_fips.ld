/* stm32u385rg_fips.ld
 *
 * Copyright (C) 2006-2026 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/*
 * STM32U385RG FIPS 140-3 Ready linker script
 *
 * Pins the wolfCrypt FIPS module boundary so the in-core integrity hash
 * (computed over [wolfCrypt_FIPS_first .. wolfCrypt_FIPS_last]) is stable
 * across rebuilds. Modeled after wolfssl/IDE/GCC-ARM/linker_fips.ld.
 *
 * Object name patterns use wildcards because the Makefile munges absolute
 * paths into object names (e.g. ..._wolfcrypt__src__aes.c.o).
 *
 * Memory:
 *   FLASH @ 0x08000000  1024K
 *   RAM   @ 0x20000000   192K
 *   RAM2  @ 0x20030000    64K
 */

ENTRY(Reset_Handler)

_estack         = ORIGIN(RAM) + LENGTH(RAM);
_Min_Heap_Size  = 0x1000;  /* 4 KB heap */
_Min_Stack_Size = 0x4000;  /* 16 KB stack - P-384 ECC needs ~10 KB */

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 192K
  RAM2  (xrw) : ORIGIN = 0x20030000, LENGTH = 64K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* ---------------------------------------------------------------
   * wolfCrypt FIPS module — pinned region.
   * Order MUST be: wolfcrypt_first, all FIPS module objects,
   * wolfcrypt_last. The integrity hash runs over this range.
   * --------------------------------------------------------------- */
  .wolfCryptFIPSModule_text :
  {
    . = ALIGN(4);
    KEEP(*wolfcrypt_first.c.o(.text .text*))
    KEEP(*wolfcrypt__src__aes.c.o(.text .text*))
    KEEP(*wolfcrypt__src__cmac.c.o(.text .text*))
    KEEP(*wolfcrypt__src__des3.c.o(.text .text*))
    KEEP(*wolfcrypt__src__dh.c.o(.text .text*))
    KEEP(*wolfcrypt__src__ecc.c.o(.text .text*))
    KEEP(*wolfcrypt__src__fips.c.o(.text .text*))
    KEEP(*wolfcrypt__src__fips_test.c.o(.text .text*))
    KEEP(*wolfcrypt__src__hmac.c.o(.text .text*))
    KEEP(*wolfcrypt__src__kdf.c.o(.text .text*))
    KEEP(*wolfcrypt__src__pwdbased.c.o(.text .text*))
    KEEP(*wolfcrypt__src__random.c.o(.text .text*))
    KEEP(*wolfcrypt__src__rsa.c.o(.text .text*))
    KEEP(*wolfcrypt__src__sha.c.o(.text .text*))
    KEEP(*wolfcrypt__src__sha256.c.o(.text .text*))
    KEEP(*wolfcrypt__src__sha3.c.o(.text .text*))
    KEEP(*wolfcrypt__src__sha512.c.o(.text .text*))
    KEEP(*wolfcrypt_last.c.o(.text .text*))
    . = ALIGN(4);
  } >FLASH

  .wolfCryptFIPSModule_rodata :
  {
    . = ALIGN(4);
    KEEP(*wolfcrypt_first.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__aes.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__cmac.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__des3.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__dh.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__ecc.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__fips.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__fips_test.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__hmac.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__kdf.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__pwdbased.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__random.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__rsa.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__sha.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__sha256.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__sha3.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt__src__sha512.c.o(.rodata .rodata*))
    KEEP(*wolfcrypt_last.c.o(.rodata .rodata*))
    . = ALIGN(4);
  } >FLASH

  /* ---------------------------------------------------------------
   * Non-FIPS wolfcrypt code + libc/libgcc helpers are pinned right
   * after the FIPS module so that relocations from inside the module
   * (sp_*.c, memcpy, memset, soft-float helpers, ...) land at
   * deterministic addresses across test / bench / app builds.
   *
   * Without this, the FIPS in-core integrity hash differs per driver
   * because cross-module branch targets shift.
   * --------------------------------------------------------------- */
  .wolfCryptNonFIPS_text :
  {
    . = ALIGN(4);
    *wolfssl*wolfcrypt__src__*.o(.text .text*)
    *wolfcrypt__src__port__st__*.o(.text .text*)
    *STM32U3xx_HAL_Driver__Src__*.o(.text .text*)
    *libc.a:*(.text .text*)
    *libgcc.a:*(.text .text*)
    *libm.a:*(.text .text*)
    . = ALIGN(4);
  } >FLASH
  .wolfCryptNonFIPS_rodata :
  {
    . = ALIGN(4);
    *wolfssl*wolfcrypt__src__*.o(.rodata .rodata*)
    *wolfcrypt__src__port__st__*.o(.rodata .rodata*)
    *STM32U3xx_HAL_Driver__Src__*.o(.rodata .rodata*)
    *libc.a:*(.rodata .rodata*)
    *libgcc.a:*(.rodata .rodata*)
    *libm.a:*(.rodata .rodata*)
    . = ALIGN(4);
  } >FLASH

  /* ---------------------------------------------------------------
   * Everything else (app, HAL, libgcc, libc) goes after.
   * --------------------------------------------------------------- */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
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
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array (READONLY) :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array (READONLY) :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  _sidata = LOADADDR(.data);

  /* FIPS module .data + .bss are pinned at the start of RAM for the
   * same reason as the .text/.rodata: relocations from inside the
   * pinned FIPS code reference globals like always_prefetch, and those
   * symbols must land at the same addresses across test/bench/app
   * builds for the in-core integrity hash to be stable. */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    KEEP(*wolfcrypt_first.c.o(.data .data*))
    KEEP(*wolfcrypt__src__aes.c.o(.data .data*))
    KEEP(*wolfcrypt__src__cmac.c.o(.data .data*))
    KEEP(*wolfcrypt__src__des3.c.o(.data .data*))
    KEEP(*wolfcrypt__src__dh.c.o(.data .data*))
    KEEP(*wolfcrypt__src__ecc.c.o(.data .data*))
    KEEP(*wolfcrypt__src__fips.c.o(.data .data*))
    KEEP(*wolfcrypt__src__fips_test.c.o(.data .data*))
    KEEP(*wolfcrypt__src__hmac.c.o(.data .data*))
    KEEP(*wolfcrypt__src__kdf.c.o(.data .data*))
    KEEP(*wolfcrypt__src__pwdbased.c.o(.data .data*))
    KEEP(*wolfcrypt__src__random.c.o(.data .data*))
    KEEP(*wolfcrypt__src__rsa.c.o(.data .data*))
    KEEP(*wolfcrypt__src__sha.c.o(.data .data*))
    KEEP(*wolfcrypt__src__sha256.c.o(.data .data*))
    KEEP(*wolfcrypt__src__sha3.c.o(.data .data*))
    KEEP(*wolfcrypt__src__sha512.c.o(.data .data*))
    KEEP(*wolfcrypt_last.c.o(.data .data*))
    /* Non-FIPS wolfcrypt + HAL globals also pinned ahead of app .data
     * so FIPS-module .text relocations to e.g. uwTickFreq stay stable. */
    *wolfssl*wolfcrypt__src__*.o(.data .data*)
    *wolfcrypt__src__port__st__*.o(.data .data*)
    *STM32U3xx_HAL_Driver__Src__*.o(.data .data*)
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
    KEEP(*wolfcrypt_first.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__aes.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__cmac.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__des3.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__dh.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__ecc.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__fips.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__fips_test.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__hmac.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__kdf.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__pwdbased.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__random.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__rsa.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__sha.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__sha256.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__sha3.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt__src__sha512.c.o(.bss .bss* COMMON))
    KEEP(*wolfcrypt_last.c.o(.bss .bss* COMMON))
    /* Non-FIPS wolfcrypt + HAL .bss before any application .bss. */
    *wolfssl*wolfcrypt__src__*.o(.bss .bss* COMMON)
    *wolfcrypt__src__port__st__*.o(.bss .bss* COMMON)
    *STM32U3xx_HAL_Driver__Src__*.o(.bss .bss* COMMON)
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

  /* libc / libm / libgcc are placed in .wolfCryptNonFIPS_text above so
   * relocations from FIPS module to those helpers stay deterministic.
   * --gc-sections will still drop unused functions from those libs. */

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
