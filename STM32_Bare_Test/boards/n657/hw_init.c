/* hw_init.c - STM32N657X0H (NUCLEO-N657X0-Q), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-N657X0-Q:
 *   - HSI 64 MHz default at reset; keep it as SYSCLK for first cut
 *     (full PLL bring-up to 600 MHz is a follow-up - mirrors the
 *     wolfboot stm32n6.c pattern: HSI/4 * 75 -> 1200 MHz VCO)
 *   - USART1 on PE5 (TX) / PE6 (RX) AF7, 115200 8N1, ST-LINK V3 VCP
 *   - No internal flash; this app links to AXISRAM2 (0x34000000) and
 *     is loaded by OpenOCD load_image, not flashed.
 *
 * Clock tree (ported from wolfBoot hal/stm32n6.c clock_config):
 *   HSI 64 MHz -> PLL1 (M=4, N=75, PDIV=1)  -> VCO 1200 MHz
 *     IC1  / 2 = 600 MHz -> CPU      (CPUSW = IC1)
 *     IC2  / 3 = 400 MHz -> AXI bus  (SYSSW = IC2)
 *     IC6  / 4 = 300 MHz -> bus C
 *     IC11 / 3 = 400 MHz -> bus D
 *     AHB prescaler / 2 -> HCLK = 200 MHz
 *     APB2 / 1 = PCLK2 = 200 MHz (USART1 kernel clock source)
 *
 * N657 silicon has CRYP + HASH + RNG + PKA. wolfSSL bare-metal HW driver
 * coverage for the N6 family is not yet wired up in stm32.c. For first-
 * cut bring-up we boot at SW baseline.
 *
 * Status (2026-05-06):
 *   - PLL bring-up + UART work end-to-end at 600 MHz CPU.
 *   - wolfcrypt_test passes through PWDBASED + AES + GCM + CCM.
 *   - The first big-int (sp_int) compute -- ecc_test() in test mode,
 *     AES bench loop in bench mode -- locks up the M55 (PC ends in
 *     Boot ROM at 0x18003514, target enters debug-unhalt-able state).
 *     Ruled OUT: heap (256K), stack (128K), CCR.UNALIGN_TRP/DIV_0_TRP,
 *     SysTick, FPU lazy stacking, I-cache + D-cache, MPU regions, all
 *     fault enables (BusFault/MemFault/UsageFault/SecureFault), GCC
 *     M55 codegen (M33 build same hang). Likely cause is TZ-M SAU
 *     configuration / Boot-ROM secure-state context expecting an FSBL-
 *     signed handoff. Proper fix is probably the SAU + secure->NS
 *     transition pattern from wolfBoot hal/armv8m_tz.h. Tracked as the
 *     next N6 follow-up.
 */

#include "stm32n657xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* The N6 has secure / non-secure peripheral aliases. We compile NS by
 * default (CMSE not enabled), so USART1 / RCC / GPIOE point at the NS
 * alias (0x4xxxxxxx). The CMSIS header maps USART1_BASE = USART1_BASE_NS
 * when CMSE is off, which matches what OpenOCD's TZ-disabled boot gives
 * us. */

/* ---- printf retarget over USART1 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0) {
        /* wait for TX FIFO space */
    }
    USART1->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
/* Bring HSI -> PLL1 -> 1200 MHz VCO -> IC1/2 = 600 MHz CPU, AXI bus 400 MHz
 * via IC2/3, HCLK = 200 MHz, PCLK2 = 200 MHz. Port of wolfBoot
 * hal/stm32n6.c clock_pll_on(). */
static void clock_init(void)
{
    uint32_t reg;

    /* 1) Make sure HSI 64 MHz is on */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->SR & RCC_SR_HSIRDY) == 0u) { }

    /* 2) Disable PLL1 before reconfiguring */
    RCC->CR &= ~RCC_CR_PLL1ON;
    while ((RCC->SR & RCC_SR_PLL1RDY) != 0u) { }

    /* 3) PLL1CFGR1: SEL=HSI, M=4, N=75, BYP=0
     *   - Boot ROM leaves PLL1BYP set, which routes HSI directly to PLL
     *     output and skips the VCO entirely. We must clear it. */
    reg = RCC->PLL1CFGR1;
    reg &= ~(RCC_PLL1CFGR1_PLL1SEL_Msk | RCC_PLL1CFGR1_PLL1DIVM_Msk |
             RCC_PLL1CFGR1_PLL1DIVN_Msk | RCC_PLL1CFGR1_PLL1BYP);
    reg |= (0u << RCC_PLL1CFGR1_PLL1SEL_Pos) |  /* SEL=000 = HSI */
           (4u << RCC_PLL1CFGR1_PLL1DIVM_Pos) |
           (75u << RCC_PLL1CFGR1_PLL1DIVN_Pos);
    RCC->PLL1CFGR1 = reg;

    /* 4) PLL1CFGR2: integer mode, no fractional. */
    RCC->PLL1CFGR2 = 0u;

    /* 5) PLL1CFGR3: PDIV1=1, PDIV2=1 -> PLL output = VCO = 1200 MHz.
     *   Disable spread spectrum + enable PLL output (PDIVEN). */
    RCC->PLL1CFGR3 = (1u << RCC_PLL1CFGR3_PLL1PDIV1_Pos) |
                     (1u << RCC_PLL1CFGR3_PLL1PDIV2_Pos) |
                     RCC_PLL1CFGR3_PLL1MODSSDIS |
                     RCC_PLL1CFGR3_PLL1MODSSRST |
                     RCC_PLL1CFGR3_PLL1PDIVEN;

    /* 6) Enable PLL1, wait for lock */
    RCC->CR |= RCC_CR_PLL1ON;
    while ((RCC->SR & RCC_SR_PLL1RDY) == 0u) { }

    /* 7) IC dividers: disable -> configure (SEL=0=PLL1, INT=N-1) -> re-enable.
     *   Bit positions in DIVENR are the same as in DIVENSR/DIVENCR. */
    /* IC1 = PLL1 / 2 = 600 MHz (CPU) */
    RCC->DIVENCR = RCC_DIVENR_IC1EN;
    RCC->IC1CFGR = ((2u - 1u) << RCC_IC1CFGR_IC1INT_Pos);
    RCC->DIVENSR = RCC_DIVENR_IC1EN;
    /* IC2 = PLL1 / 3 = 400 MHz (AXI bus) */
    RCC->DIVENCR = RCC_DIVENR_IC2EN;
    RCC->IC2CFGR = ((3u - 1u) << RCC_IC2CFGR_IC2INT_Pos);
    RCC->DIVENSR = RCC_DIVENR_IC2EN;
    /* IC6 = PLL1 / 4 = 300 MHz (bus C) */
    RCC->DIVENCR = RCC_DIVENR_IC6EN;
    RCC->IC6CFGR = ((4u - 1u) << RCC_IC6CFGR_IC6INT_Pos);
    RCC->DIVENSR = RCC_DIVENR_IC6EN;
    /* IC11 = PLL1 / 3 = 400 MHz (bus D) */
    RCC->DIVENCR = RCC_DIVENR_IC11EN;
    RCC->IC11CFGR = ((3u - 1u) << RCC_IC11CFGR_IC11INT_Pos);
    RCC->DIVENSR = RCC_DIVENR_IC11EN;

    /* 8) AHB prescaler /2 -> HCLK = AXI(400) / 2 = 200 MHz */
    reg = RCC->CFGR2;
    reg &= ~RCC_CFGR2_HPRE_Msk;
    reg |= (1u << RCC_CFGR2_HPRE_Pos);
    RCC->CFGR2 = reg;

    /* 9) Switch CPU to IC1 (CPUSW=11), system bus to IC2/IC6/IC11
     *    (SYSSW=11). Both fields are 2-bit; 0x3 = use IC dividers. */
    reg = RCC->CFGR1;
    reg &= ~(RCC_CFGR1_CPUSW_Msk | RCC_CFGR1_SYSSW_Msk);
    reg |= (0x3u << RCC_CFGR1_CPUSW_Pos) |
           (0x3u << RCC_CFGR1_SYSSW_Pos);
    RCC->CFGR1 = reg;
    while ((RCC->CFGR1 & RCC_CFGR1_CPUSWS_Msk) !=
           (0x3u << RCC_CFGR1_CPUSWS_Pos)) { }
    while ((RCC->CFGR1 & RCC_CFGR1_SYSSWS_Msk) !=
           (0x3u << RCC_CFGR1_SYSSWS_Pos)) { }

    /* CPU now runs from IC1 = PLL1/2 = 600 MHz. SystemInit left
     * SystemCoreClock at the 64 MHz HSI default; re-derive it from the
     * RCC switch status so consumers (and the banner) see the real CPUCLK. */
    SystemCoreClockUpdate();
}

/* ---- USART1 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* Enable GPIOE on AHB4 */
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
    (void)RCC->AHB4ENR;

    /* PE5 (TX), PE6 (RX): MODER=AF (10b), AF7 (USART1) */
    GPIOE->MODER &= ~(GPIO_MODER_MODE5_Msk | GPIO_MODER_MODE6_Msk);
    GPIOE->MODER |= (2u << GPIO_MODER_MODE5_Pos) | (2u << GPIO_MODER_MODE6_Pos);
    GPIOE->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED5_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED6_Pos);
    /* AFR[0] holds AFs for pins 0..7 */
    GPIOE->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL5_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL6_Pos));
    GPIOE->AFR[0] |= (7u << GPIO_AFRL_AFSEL5_Pos) |
                     (7u << GPIO_AFRL_AFSEL6_Pos);

    /* Enable USART1 clock (APB2ENR bit USART1EN) */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    /* USART1: 8N1, oversample 16. PCLK2 = 200 MHz (Boot ROM PLL1/IC2
     * gives HCLK 600/2/1 = 200 MHz on APB2). */
    USART1->CR1 = 0;
    USART1->BRR = 200000000u / 115200u;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        /* spin */
    }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* Clear any MPU regions the Boot ROM may have left configured. The N6
     * Boot ROM hands off with the MPU enabled to protect its own state;
     * a no-execute or read-only region overlapping our LRUN code or our
     * heap will fault deep inside wolfssl rather than at startup. Clear
     * MPU before doing anything else, while the FPU/SysTick/UART are
     * still off. */
    MPU->CTRL = 0u;
    __DSB();
    __ISB();

    /* Clear CCR.UNALIGN_TRP and CCR.DIV_0_TRP -- Boot ROM may leave them
     * set, and wolfSSL SP-math has unaligned-access patterns that should
     * be allowed (M55 supports unaligned access; trap-on-unaligned is a
     * debug aid, not a runtime protection). */
    SCB->CCR &= ~(SCB_CCR_UNALIGN_TRP_Msk | SCB_CCR_DIV_0_TRP_Msk);
    __DSB();
    __ISB();

    /* FPU CP10/CP11 full access (Cortex-M55 has FP) */
    SCB->CPACR |= (0xFu << 20);
    /* Disable FPU lazy stacking -- on the U5/H5 line lazy-stack-on with
     * the wrong stack frame caused LSPERR; mirror that conservatively. */
    FPU->FPCCR &= ~(FPU_FPCCR_LSPEN_Msk | FPU_FPCCR_ASPEN_Msk);
    __DSB();
    __ISB();

    /* Enable BusFault / MemFault / UsageFault / SecureFault so escalation
     * goes through our handlers instead of straight to HardFault or to a
     * Boot-ROM lockup. M55 boots with these disabled. */
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk |
                  SCB_SHCSR_MEMFAULTENA_Msk |
                  SCB_SHCSR_USGFAULTENA_Msk |
                  SCB_SHCSR_SECUREFAULTENA_Msk;

    /* Enable I-cache and D-cache. M55 boots with caches off; the AES /
     * GCM / ECC inner loops do thousands of close-address reads that
     * hang the bus without cache. */
    SCB_EnableICache();
    SCB_EnableDCache();

    SystemInit();
    clock_init();
    uart_init();
    /* HCLK is 200 MHz post-PLL (AXI=400 / HPRE=2). SysTick CLKSOURCE=1
     * uses CPUCLK = 600 MHz for the count source, so /1000 ticks per ms. */
    board_common_systick_init(600000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 600000000u;
}


const char *board_name(void)
{
    return "NUCLEO-N657X0-Q";
}
