/* startup_stm32f437xx.s
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * This file is part of wolfssl-examples.
 *
 * wolfssl-examples is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfssl-examples is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/*
 * Cortex-M4 vector table and reset handler for STM32F437xx.
 *
 * Linker-script symbols consumed:
 *   _estack   top of stack (initial MSP)
 *   _sidata   load address of .data in flash
 *   _sdata    start of .data in RAM
 *   _edata    end   of .data in RAM
 *   _sbss     start of .bss  in RAM
 *   _ebss     end   of .bss  in RAM
 */

    .syntax unified
    .cpu    cortex-m4
    .fpu    fpv4-sp-d16
    .thumb

    .global g_pfnVectors
    .global Default_Handler
    .global Reset_Handler

/* ---------------------------------------------------------------------- */
/* Reset handler                                                          */
/* ---------------------------------------------------------------------- */
    .section .text.Reset_Handler
    .weak    Reset_Handler
    .type    Reset_Handler, %function
Reset_Handler:
    /* Set up MSP early in case the boot ROM left it elsewhere. */
    ldr     r0, =_estack
    mov     sp, r0

    /* Copy .data from flash (_sidata) to RAM (_sdata.._edata). */
    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_sidata
1:
    cmp     r0, r1
    bcs     2f
    ldr     r4, [r2]
    str     r4, [r0]
    adds    r2, r2, #4
    adds    r0, r0, #4
    b       1b
2:

    /* Zero .bss (_sbss.._ebss). */
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r2, #0
3:
    cmp     r0, r1
    bcs     4f
    str     r2, [r0]
    adds    r0, r0, #4
    b       3b
4:

    /* CMSIS-style early init: FPU enable, optional VTOR relocation. */
    bl      SystemInit

    /* Run C++/C constructors (newlib). */
    bl      __libc_init_array

    /* Hand off to application. */
    bl      main

    /* main() should not return; if it does, hang. */
5:
    b       5b
    .size   Reset_Handler, .-Reset_Handler

/* ---------------------------------------------------------------------- */
/* Default handler — catches any unhandled IRQ; spin so a debugger sees   */
/* the offending vector by inspecting the stacked PC.                     */
/* ---------------------------------------------------------------------- */
    .section .text.Default_Handler,"ax",%progbits
    .type    Default_Handler, %function
Default_Handler:
    b       .
    .size   Default_Handler, .-Default_Handler

/* ---------------------------------------------------------------------- */
/* Vector table                                                           */
/*                                                                        */
/* Layout: ARMv7-M defines slots 0..15 (stack + reset + 14 system         */
/* exceptions). Slots 16+ are device-specific external interrupts. The    */
/* STM32F437 has 91 external IRQ slots (0..90), with the last named       */
/* entry being DMA2D at position 90. Names match the CMSIS device header  */
/* stm32f437xx.h so application code can reference them by name.          */
/* ---------------------------------------------------------------------- */
    .section .isr_vector,"a",%progbits
    .type    g_pfnVectors, %object
g_pfnVectors:
    /* ---- ARMv7-M core vectors (0..15) ---- */
    .word   _estack                    /*  0 Initial MSP                 */
    .word   Reset_Handler              /*  1 Reset                       */
    .word   NMI_Handler                /*  2 NMI                         */
    .word   HardFault_Handler          /*  3 Hard fault                  */
    .word   MemManage_Handler          /*  4 MPU fault                   */
    .word   BusFault_Handler           /*  5 Bus fault                   */
    .word   UsageFault_Handler         /*  6 Usage fault                 */
    .word   0                          /*  7 reserved                    */
    .word   0                          /*  8 reserved                    */
    .word   0                          /*  9 reserved                    */
    .word   0                          /* 10 reserved                    */
    .word   SVC_Handler                /* 11 SVCall                      */
    .word   DebugMon_Handler           /* 12 Debug monitor               */
    .word   0                          /* 13 reserved                    */
    .word   PendSV_Handler             /* 14 PendSV                      */
    .word   SysTick_Handler            /* 15 SysTick                     */

    /* ---- STM32F437 external IRQs (positions 0..90) ---- */
    .word   WWDG_IRQHandler                 /*  0 Window watchdog        */
    .word   PVD_IRQHandler                  /*  1 PVD via EXTI            */
    .word   TAMP_STAMP_IRQHandler           /*  2 Tamper / TimeStamp      */
    .word   RTC_WKUP_IRQHandler             /*  3 RTC wakeup              */
    .word   FLASH_IRQHandler                /*  4 Flash global            */
    .word   RCC_IRQHandler                  /*  5 RCC global              */
    .word   EXTI0_IRQHandler                /*  6 EXTI line 0             */
    .word   EXTI1_IRQHandler                /*  7 EXTI line 1             */
    .word   EXTI2_IRQHandler                /*  8 EXTI line 2             */
    .word   EXTI3_IRQHandler                /*  9 EXTI line 3             */
    .word   EXTI4_IRQHandler                /* 10 EXTI line 4             */
    .word   DMA1_Stream0_IRQHandler         /* 11 DMA1 stream 0           */
    .word   DMA1_Stream1_IRQHandler         /* 12 DMA1 stream 1           */
    .word   DMA1_Stream2_IRQHandler         /* 13 DMA1 stream 2           */
    .word   DMA1_Stream3_IRQHandler         /* 14 DMA1 stream 3           */
    .word   DMA1_Stream4_IRQHandler         /* 15 DMA1 stream 4           */
    .word   DMA1_Stream5_IRQHandler         /* 16 DMA1 stream 5           */
    .word   DMA1_Stream6_IRQHandler         /* 17 DMA1 stream 6           */
    .word   ADC_IRQHandler                  /* 18 ADC1/2/3 global         */
    .word   CAN1_TX_IRQHandler              /* 19 CAN1 TX                 */
    .word   CAN1_RX0_IRQHandler             /* 20 CAN1 RX0                */
    .word   CAN1_RX1_IRQHandler             /* 21 CAN1 RX1                */
    .word   CAN1_SCE_IRQHandler             /* 22 CAN1 SCE                */
    .word   EXTI9_5_IRQHandler              /* 23 EXTI lines 5..9         */
    .word   TIM1_BRK_TIM9_IRQHandler        /* 24 TIM1 break / TIM9       */
    .word   TIM1_UP_TIM10_IRQHandler        /* 25 TIM1 update / TIM10     */
    .word   TIM1_TRG_COM_TIM11_IRQHandler   /* 26 TIM1 trg/com / TIM11    */
    .word   TIM1_CC_IRQHandler              /* 27 TIM1 capture compare    */
    .word   TIM2_IRQHandler                 /* 28 TIM2                    */
    .word   TIM3_IRQHandler                 /* 29 TIM3                    */
    .word   TIM4_IRQHandler                 /* 30 TIM4                    */
    .word   I2C1_EV_IRQHandler              /* 31 I2C1 event              */
    .word   I2C1_ER_IRQHandler              /* 32 I2C1 error              */
    .word   I2C2_EV_IRQHandler              /* 33 I2C2 event              */
    .word   I2C2_ER_IRQHandler              /* 34 I2C2 error              */
    .word   SPI1_IRQHandler                 /* 35 SPI1                    */
    .word   SPI2_IRQHandler                 /* 36 SPI2                    */
    .word   USART1_IRQHandler               /* 37 USART1                  */
    .word   USART2_IRQHandler               /* 38 USART2                  */
    .word   USART3_IRQHandler               /* 39 USART3                  */
    .word   EXTI15_10_IRQHandler            /* 40 EXTI lines 10..15       */
    .word   RTC_Alarm_IRQHandler            /* 41 RTC alarms              */
    .word   OTG_FS_WKUP_IRQHandler          /* 42 OTG FS wakeup           */
    .word   TIM8_BRK_TIM12_IRQHandler       /* 43 TIM8 break / TIM12      */
    .word   TIM8_UP_TIM13_IRQHandler        /* 44 TIM8 update / TIM13     */
    .word   TIM8_TRG_COM_TIM14_IRQHandler   /* 45 TIM8 trg/com / TIM14    */
    .word   TIM8_CC_IRQHandler              /* 46 TIM8 capture compare    */
    .word   DMA1_Stream7_IRQHandler         /* 47 DMA1 stream 7           */
    .word   FMC_IRQHandler                  /* 48 FMC                     */
    .word   SDIO_IRQHandler                 /* 49 SDIO                    */
    .word   TIM5_IRQHandler                 /* 50 TIM5                    */
    .word   SPI3_IRQHandler                 /* 51 SPI3                    */
    .word   UART4_IRQHandler                /* 52 UART4                   */
    .word   UART5_IRQHandler                /* 53 UART5                   */
    .word   TIM6_DAC_IRQHandler             /* 54 TIM6 / DAC underrun     */
    .word   TIM7_IRQHandler                 /* 55 TIM7                    */
    .word   DMA2_Stream0_IRQHandler         /* 56 DMA2 stream 0           */
    .word   DMA2_Stream1_IRQHandler         /* 57 DMA2 stream 1           */
    .word   DMA2_Stream2_IRQHandler         /* 58 DMA2 stream 2           */
    .word   DMA2_Stream3_IRQHandler         /* 59 DMA2 stream 3           */
    .word   DMA2_Stream4_IRQHandler         /* 60 DMA2 stream 4           */
    .word   ETH_IRQHandler                  /* 61 Ethernet                */
    .word   ETH_WKUP_IRQHandler             /* 62 Ethernet wakeup         */
    .word   CAN2_TX_IRQHandler              /* 63 CAN2 TX                 */
    .word   CAN2_RX0_IRQHandler             /* 64 CAN2 RX0                */
    .word   CAN2_RX1_IRQHandler             /* 65 CAN2 RX1                */
    .word   CAN2_SCE_IRQHandler             /* 66 CAN2 SCE                */
    .word   OTG_FS_IRQHandler               /* 67 USB OTG FS              */
    .word   DMA2_Stream5_IRQHandler         /* 68 DMA2 stream 5           */
    .word   DMA2_Stream6_IRQHandler         /* 69 DMA2 stream 6           */
    .word   DMA2_Stream7_IRQHandler         /* 70 DMA2 stream 7           */
    .word   USART6_IRQHandler               /* 71 USART6                  */
    .word   I2C3_EV_IRQHandler              /* 72 I2C3 event              */
    .word   I2C3_ER_IRQHandler              /* 73 I2C3 error              */
    .word   OTG_HS_EP1_OUT_IRQHandler       /* 74 OTG HS endpoint 1 out   */
    .word   OTG_HS_EP1_IN_IRQHandler        /* 75 OTG HS endpoint 1 in    */
    .word   OTG_HS_WKUP_IRQHandler          /* 76 OTG HS wakeup           */
    .word   OTG_HS_IRQHandler               /* 77 USB OTG HS              */
    .word   DCMI_IRQHandler                 /* 78 DCMI                    */
    .word   CRYP_IRQHandler                 /* 79 CRYP                    */
    .word   HASH_RNG_IRQHandler             /* 80 HASH and RNG            */
    .word   FPU_IRQHandler                  /* 81 FPU                     */
    .word   UART7_IRQHandler                /* 82 UART7                   */
    .word   UART8_IRQHandler                /* 83 UART8                   */
    .word   SPI4_IRQHandler                 /* 84 SPI4                    */
    .word   SPI5_IRQHandler                 /* 85 SPI5                    */
    .word   SPI6_IRQHandler                 /* 86 SPI6                    */
    .word   SAI1_IRQHandler                 /* 87 SAI1                    */
    .word   0                               /* 88 reserved                */
    .word   0                               /* 89 reserved                */
    .word   DMA2D_IRQHandler                /* 90 DMA2D                   */
    .size   g_pfnVectors, .-g_pfnVectors

/* ---------------------------------------------------------------------- */
/* Weak handler aliases — every named slot defaults to Default_Handler.   */
/* Application code can override any of these by defining a strong        */
/* function with the same name (e.g. SysTick_Handler in stm32f4xx_it.c).  */
/* ---------------------------------------------------------------------- */
    .macro  weak_alias name
    .weak   \name
    .thumb_set \name, Default_Handler
    .endm

    /* Core exceptions */
    weak_alias NMI_Handler
    weak_alias HardFault_Handler
    weak_alias MemManage_Handler
    weak_alias BusFault_Handler
    weak_alias UsageFault_Handler
    weak_alias SVC_Handler
    weak_alias DebugMon_Handler
    weak_alias PendSV_Handler
    weak_alias SysTick_Handler

    /* External IRQs */
    weak_alias WWDG_IRQHandler
    weak_alias PVD_IRQHandler
    weak_alias TAMP_STAMP_IRQHandler
    weak_alias RTC_WKUP_IRQHandler
    weak_alias FLASH_IRQHandler
    weak_alias RCC_IRQHandler
    weak_alias EXTI0_IRQHandler
    weak_alias EXTI1_IRQHandler
    weak_alias EXTI2_IRQHandler
    weak_alias EXTI3_IRQHandler
    weak_alias EXTI4_IRQHandler
    weak_alias DMA1_Stream0_IRQHandler
    weak_alias DMA1_Stream1_IRQHandler
    weak_alias DMA1_Stream2_IRQHandler
    weak_alias DMA1_Stream3_IRQHandler
    weak_alias DMA1_Stream4_IRQHandler
    weak_alias DMA1_Stream5_IRQHandler
    weak_alias DMA1_Stream6_IRQHandler
    weak_alias ADC_IRQHandler
    weak_alias CAN1_TX_IRQHandler
    weak_alias CAN1_RX0_IRQHandler
    weak_alias CAN1_RX1_IRQHandler
    weak_alias CAN1_SCE_IRQHandler
    weak_alias EXTI9_5_IRQHandler
    weak_alias TIM1_BRK_TIM9_IRQHandler
    weak_alias TIM1_UP_TIM10_IRQHandler
    weak_alias TIM1_TRG_COM_TIM11_IRQHandler
    weak_alias TIM1_CC_IRQHandler
    weak_alias TIM2_IRQHandler
    weak_alias TIM3_IRQHandler
    weak_alias TIM4_IRQHandler
    weak_alias I2C1_EV_IRQHandler
    weak_alias I2C1_ER_IRQHandler
    weak_alias I2C2_EV_IRQHandler
    weak_alias I2C2_ER_IRQHandler
    weak_alias SPI1_IRQHandler
    weak_alias SPI2_IRQHandler
    weak_alias USART1_IRQHandler
    weak_alias USART2_IRQHandler
    weak_alias USART3_IRQHandler
    weak_alias EXTI15_10_IRQHandler
    weak_alias RTC_Alarm_IRQHandler
    weak_alias OTG_FS_WKUP_IRQHandler
    weak_alias TIM8_BRK_TIM12_IRQHandler
    weak_alias TIM8_UP_TIM13_IRQHandler
    weak_alias TIM8_TRG_COM_TIM14_IRQHandler
    weak_alias TIM8_CC_IRQHandler
    weak_alias DMA1_Stream7_IRQHandler
    weak_alias FMC_IRQHandler
    weak_alias SDIO_IRQHandler
    weak_alias TIM5_IRQHandler
    weak_alias SPI3_IRQHandler
    weak_alias UART4_IRQHandler
    weak_alias UART5_IRQHandler
    weak_alias TIM6_DAC_IRQHandler
    weak_alias TIM7_IRQHandler
    weak_alias DMA2_Stream0_IRQHandler
    weak_alias DMA2_Stream1_IRQHandler
    weak_alias DMA2_Stream2_IRQHandler
    weak_alias DMA2_Stream3_IRQHandler
    weak_alias DMA2_Stream4_IRQHandler
    weak_alias ETH_IRQHandler
    weak_alias ETH_WKUP_IRQHandler
    weak_alias CAN2_TX_IRQHandler
    weak_alias CAN2_RX0_IRQHandler
    weak_alias CAN2_RX1_IRQHandler
    weak_alias CAN2_SCE_IRQHandler
    weak_alias OTG_FS_IRQHandler
    weak_alias DMA2_Stream5_IRQHandler
    weak_alias DMA2_Stream6_IRQHandler
    weak_alias DMA2_Stream7_IRQHandler
    weak_alias USART6_IRQHandler
    weak_alias I2C3_EV_IRQHandler
    weak_alias I2C3_ER_IRQHandler
    weak_alias OTG_HS_EP1_OUT_IRQHandler
    weak_alias OTG_HS_EP1_IN_IRQHandler
    weak_alias OTG_HS_WKUP_IRQHandler
    weak_alias OTG_HS_IRQHandler
    weak_alias DCMI_IRQHandler
    weak_alias CRYP_IRQHandler
    weak_alias HASH_RNG_IRQHandler
    weak_alias FPU_IRQHandler
    weak_alias UART7_IRQHandler
    weak_alias UART8_IRQHandler
    weak_alias SPI4_IRQHandler
    weak_alias SPI5_IRQHandler
    weak_alias SPI6_IRQHandler
    weak_alias SAI1_IRQHandler
    weak_alias DMA2D_IRQHandler

    .end
