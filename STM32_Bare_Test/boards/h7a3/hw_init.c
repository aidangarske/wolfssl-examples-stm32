/* hw_init.c - STM32H7A3ZI-Q (NUCLEO-H7A3ZI-Q), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-H7A3ZI-Q:
 *   - HSI 64 MHz -> PLL1 (M=4, N=35, P=2) -> SYSCLK 280 MHz.
 *     AHB /1 (HCLK 280 MHz), APB1/2/3/4 /2 (140 MHz). VOS Scale 0
 *     (PWR_SRDCR.VOS = 11b), FLASH 6 WS + WRHIGHFREQ=11b.
 *   - USART3 on PD8 (TX) / PD9 (RX) AF7 -- ST-LINK V3 VCP on the
 *     NUCLEO-H7A3ZI-Q (same Nucleo-144 layout as H723/H753).
 *   - Cortex-M7.
 *
 * H7A3xx silicon HW crypto: RNG only.
 *
 * Bring-up sequence mirrors what STM32CubeIDE's HAL generator emits
 * for this exact board (verified against a known-working CubeIDE
 * project on the bench):
 *
 *   1) Commit the power supply choice AT RUNTIME (NOT from startup).
 *      The chip POR state is "Run* mode" -- PWR_CR3 = 0x06
 *      (LDOEN | SMPSEN both pending) and ACTVOSRDY = 0. The
 *      HAL_PWREx_ConfigSupply() flow is to write PWR_CR3 with the
 *      requested supply (here SMPS direct), then wait on
 *      PWR_CSR1.ACTVOSRDY (NOT PWR_SRDCR.VOSRDY -- that one is for
 *      the later VOS-scale-transition phase). Doing this at runtime
 *      instead of from startup_*.s ExitRun0Mode() means that if the
 *      supply commit hangs the chip can fall back to plain HSI
 *      64 MHz with SWD still accessible (instead of bricking SWD
 *      until USB power-cycle, which is what happens if the same
 *      sequence runs from startup).
 *
 *   2) Set VOS Scale 0 (max perf, licenses 280 MHz). Wait on
 *      PWR_SRDCR.VOSRDY (the VOS-transition-complete flag for H7A3,
 *      separate from PWR_CSR1.ACTVOSRDY which is the supply-commit
 *      flag).
 *
 *   3) FLASH latency 6 WS + WRHIGHFREQ = 11b for 280 MHz at VOS0.
 *
 *   4) PLL1: HSI 64 MHz source / M=4 -> 16 MHz ref, N=35 -> 560 MHz
 *      VCO, /P=2 -> 280 MHz SYSCLK. PLL1RGE = 11b (8-16 MHz input
 *      range). PLL1VCOSEL = 0 (wide range 192-836 MHz).
 *
 *   5) Pre-set AHB/APB prescalers BEFORE switching SYSCLK to PLL1
 *      so no peripheral domain transiently overclocks.
 *
 *   6) Switch SYSCLK source to PLL1, reprogram USART3 BRR for the
 *      new PCLK1 (140 MHz).
 *
 *   7) Enable HSI48 for RNG kernel clock.
 *
 * Stepwise UART debug output frames the bring-up so any future
 * hang reproduces with a single-line annotation of the last
 * successful step.
 */

#include "stm32h7xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

#define BOARD_SYSCLK_HZ 280000000u
#define BOARD_PCLK1_HZ  140000000u
#define HSI_DEFAULT_HZ   64000000u

/* H7A3 PWR_SRDCR.VOS Scale 0 = bits 14+15 set (max performance,
 * licenses 280 MHz operation). Matches HAL PWR_REGULATOR_VOLTAGE_SCALE0
 * which is (PWR_SRDCR_VOS_1 | PWR_SRDCR_VOS_0). */
#define H7A3_VOS_SCALE_0  (PWR_SRDCR_VOS_1 | PWR_SRDCR_VOS_0)

/* Supply commit value for SMPS direct mode -- matches HAL
 * PWR_DIRECT_SMPS_SUPPLY which is just the SMPSEN bit. */
#define H7A3_SUPPLY_SMPS_DIRECT   PWR_CR3_SMPSEN

/* PWR_CR3 fields whose state determines the "current supply config"
 * for the HAL_PWREx_ConfigSupply() check. Mirrors HAL's
 * PWR_SUPPLY_CONFIG_MASK on the SMPS-defined Q variant. */
#define H7A3_SUPPLY_CONFIG_MASK   (PWR_CR3_SMPSLEVEL | PWR_CR3_SMPSEXTHP | \
                                   PWR_CR3_SMPSEN | PWR_CR3_LDOEN |       \
                                   PWR_CR3_BYPASS)

/* ---- Polled USART3 helpers (used at HSI 64 MHz then at PCLK1 140 MHz). */
void board_putc(int ch)
{
    while ((USART3->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    USART3->TDR = (uint32_t)ch & 0xFFu;
}

static void board_puts(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n') {
            board_putc('\r');
        }
        board_putc(*s++);
    }
}

static void uart_pins_init(void)
{
    /* GPIOD clock + USART3 kernel clock = PCLK1 (default). */
    RCC->AHB4ENR    |= RCC_AHB4ENR_GPIODEN;
    RCC->CDCCIP2R   &= ~RCC_CDCCIP2R_USART234578SEL_Msk;
    RCC->APB1LENR   |= RCC_APB1LENR_USART3EN;
    (void)RCC->APB1LENR;

    /* PD8 (TX) / PD9 (RX) AF7 for USART3. BRR is programmed separately
     * by uart_program_brr() so we can reprogram it after a PLL switch. */
    board_common_uart_pin_init(GPIOD, 8u, 9u, 7u);
}

static void uart_program_brr(uint32_t kernel_clk_hz)
{
    board_common_uart_basic_init(USART3, kernel_clk_hz, 115200u);
}

/* Drain TX FIFO + wait for the last byte to fully shift out at the
 * CURRENT BRR. Must be called before changing PCLK1 (= USART kernel
 * clock) -- otherwise in-flight chars get clocked at the new rate
 * and arrive garbled on the host terminal. */
static void uart_drain_tx(void)
{
    while ((USART3->ISR & USART_ISR_TC) == 0u) { }
}

/* Bounded wait. Returns 0 on success, -1 on timeout. */
static int wait_bit_set(volatile uint32_t *reg, uint32_t mask, uint32_t loops)
{
    while (loops != 0u) {
        if ((*reg & mask) != 0u) {
            return 0;
        }
        loops--;
    }
    return -1;
}

#define WAIT_LOOPS 4000000u

static int clock_init_pll280(void)
{
    uint32_t reg;

    /* ---- 1) Commit supply config (Run* -> normal Run) ---------------
     * Mirrors HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY): if the
     * supply bits already match (chip has been here before), just
     * verify and proceed. Otherwise write the new supply config and
     * wait for PWR_CSR1.ACTVOSRDY to assert -- this is the flag that
     * signals exit from Run* mode. */
    if ((PWR->CR3 & (PWR_CR3_SMPSEN | PWR_CR3_LDOEN | PWR_CR3_BYPASS)) ==
        (PWR_CR3_SMPSEN | PWR_CR3_LDOEN)) {
        /* Reset state: supply not yet committed. Write SMPS direct. */
        board_puts("[H7A3] PWR commit SMPS direct...\n");
        reg = PWR->CR3;
        reg &= ~H7A3_SUPPLY_CONFIG_MASK;
        reg |= H7A3_SUPPLY_SMPS_DIRECT;
        PWR->CR3 = reg;
    }
    else {
        /* PWR_CR3 is already committed (must match SMPS-direct since
         * that's what we ask for; mismatch returns error in HAL too). */
        if ((PWR->CR3 & H7A3_SUPPLY_CONFIG_MASK) !=
            H7A3_SUPPLY_SMPS_DIRECT) {
            board_puts("[H7A3] FAIL: PWR_CR3 locked to different supply\n");
            return -1;
        }
        board_puts("[H7A3] PWR already committed (SMPS direct)\n");
    }
    if (wait_bit_set(&PWR->CSR1, PWR_CSR1_ACTVOSRDY, WAIT_LOOPS) != 0) {
        board_puts("[H7A3] FAIL: ACTVOSRDY (supply commit) timeout\n");
        return -1;
    }
    board_puts("[H7A3] supply OK, ACTVOSRDY=1\n");

    /* ---- 2) VOS Scale 0 (licenses 280 MHz) -------------------------- */
    board_puts("[H7A3] VOS Scale 0...\n");
    reg = PWR->SRDCR;
    reg &= ~PWR_SRDCR_VOS_Msk;
    reg |= H7A3_VOS_SCALE_0;
    PWR->SRDCR = reg;
    if (wait_bit_set(&PWR->SRDCR, PWR_SRDCR_VOSRDY, WAIT_LOOPS) != 0) {
        board_puts("[H7A3] FAIL: SRDCR.VOSRDY (Scale 0) timeout\n");
        return -1;
    }
    board_puts("[H7A3] VOS Scale 0 RDY\n");

    /* ---- 3) FLASH 6 WS, WRHIGHFREQ=11b for 280 MHz / VOS0 ----------- */
    FLASH->ACR = (FLASH->ACR & ~(FLASH_ACR_LATENCY_Msk |
                                  FLASH_ACR_WRHIGHFREQ_Msk))
               | (6u << FLASH_ACR_LATENCY_Pos)
               | (3u << FLASH_ACR_WRHIGHFREQ_Pos);
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) !=
           (6u << FLASH_ACR_LATENCY_Pos)) { }
    board_puts("[H7A3] FLASH 6 WS; reconfiguring clock tree...\n");

    /* Drain UART before any clock-tree change. The APB1 prescaler
     * write below switches PCLK1 (and the USART3 kernel clock) from
     * /1 to /2, so any byte still in the TX shifter would arrive
     * garbled. Suppress intermediate "[H7A3] ..." progress prints
     * across the prescaler+PLL+SYSCLK transition and only print
     * again once BRR has been re-programmed for the new PCLK1. */
    uart_drain_tx();

    /* ---- 4) Make sure HSI is on as a stable SYSCLK source while
     *        we reconfigure PLL1. Also force CFGR.SW=HSI in case
     *        anything previously switched the source. */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | (0u << RCC_CFGR_SW_Pos);

    /* ---- 5) AHB/APB prescalers BEFORE switching to PLL so no
     *        peripheral domain transiently overclocks. AHB /1
     *        (HCLK = SYSCLK = 280 MHz), APB1/2/3/4 /2 (140 MHz).
     *        H7A3 uses CDCFGR1/CDCFGR2/SRDCFGR (CD/SRD domains). */
    RCC->CDCFGR1 = (RCC->CDCFGR1 & ~(RCC_CDCFGR1_HPRE_Msk |
                                       RCC_CDCFGR1_CDPPRE_Msk |
                                       RCC_CDCFGR1_CDCPRE_Msk))
                 | (0x0u << RCC_CDCFGR1_HPRE_Pos)    /* HCLK = SYSCLK /1 */
                 | (0x4u << RCC_CDCFGR1_CDPPRE_Pos)  /* APB3 /2 */
                 | (0x0u << RCC_CDCFGR1_CDCPRE_Pos); /* SYSCLK /1 */
    RCC->CDCFGR2 = (RCC->CDCFGR2 & ~(RCC_CDCFGR2_CDPPRE1_Msk |
                                       RCC_CDCFGR2_CDPPRE2_Msk))
                 | (0x4u << RCC_CDCFGR2_CDPPRE1_Pos)  /* APB1 /2 */
                 | (0x4u << RCC_CDCFGR2_CDPPRE2_Pos); /* APB2 /2 */
    RCC->SRDCFGR = (RCC->SRDCFGR & ~RCC_SRDCFGR_SRDPPRE_Msk)
                 | (0x4u << RCC_SRDCFGR_SRDPPRE_Pos); /* APB4 /2 */

    /* ---- 6) PLL1 config: HSI source, M=4 (64/4 = 16 MHz ref),
     *        N=35 (16 * 35 = 560 MHz VCO), P=2 (560/2 = 280 MHz),
     *        Q=4 (140 MHz), R=2. PLL1RGE = 11b (8-16 MHz input
     *        range -- correct for 16 MHz ref). PLL1VCOSEL = 0
     *        (wide range, 192-836 MHz; 560 MHz VCO fits). */
    RCC->PLLCKSELR = (RCC->PLLCKSELR & ~(RCC_PLLCKSELR_PLLSRC_Msk |
                                          RCC_PLLCKSELR_DIVM1_Msk))
                   | (0x0u << RCC_PLLCKSELR_PLLSRC_Pos)   /* HSI */
                   | (0x4u << RCC_PLLCKSELR_DIVM1_Pos);    /* M=4 */

    RCC->PLL1DIVR = ((35u  - 1u) << RCC_PLL1DIVR_N1_Pos)
                  | ((2u   - 1u) << RCC_PLL1DIVR_P1_Pos)
                  | ((4u   - 1u) << RCC_PLL1DIVR_Q1_Pos)
                  | ((2u   - 1u) << RCC_PLL1DIVR_R1_Pos);

    reg = RCC->PLLCFGR;
    reg &= ~(RCC_PLLCFGR_PLL1RGE_Msk | RCC_PLLCFGR_PLL1VCOSEL_Msk);
    reg |= (0x3u << RCC_PLLCFGR_PLL1RGE_Pos)  /* 8-16 MHz input */
        |  RCC_PLLCFGR_DIVP1EN
        |  RCC_PLLCFGR_DIVQ1EN
        |  RCC_PLLCFGR_DIVR1EN;
    RCC->PLLCFGR = reg;

    /* PLL1 on, wait for lock. No prints across this phase (UART
     * kernel clock is mid-transition; see uart_drain_tx call above). */
    RCC->CR |= RCC_CR_PLL1ON;
    if (wait_bit_set(&RCC->CR, RCC_CR_PLL1RDY, WAIT_LOOPS) != 0) {
        /* PCLK1 is now 32 MHz (HSI 64 MHz / APB1 /2). Reprogram BRR
         * so the failure message actually lands on the console. */
        uart_program_brr(HSI_DEFAULT_HZ / 2u);
        board_puts("[H7A3] FAIL: PLL1RDY timeout\n");
        return -1;
    }

    /* ---- 7) Switch SYSCLK to PLL1 (CFGR.SW = 11b). */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) |
                (0x3u << RCC_CFGR_SW_Pos);
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos) != 0x3u) { }

    /* USART BRR is stale for the new PCLK1; reprogram. */
    uart_program_brr(BOARD_PCLK1_HZ);
    board_puts("[H7A3] PLL1 on, SYSCLK = 280 MHz\n");

    /* ---- 8) HSI48 for RNG kernel clock. */
    RCC->CR |= RCC_CR_HSI48ON;
    while ((RCC->CR & RCC_CR_HSI48RDY) == 0u) { }
    board_puts("[H7A3] HSI48 RDY\n");

    return 0;
}

/* ---- Public board API ------------------------------------------------- */
static uint32_t s_sysclk_hz = HSI_DEFAULT_HZ;

void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M7F) */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();

    /* Bring up UART at HSI 64 MHz first so every PLL step is traced. */
    uart_pins_init();
    uart_program_brr(HSI_DEFAULT_HZ);
    board_puts("\n\n[H7A3] BARE init, HSI 64 MHz\n");

    if (clock_init_pll280() == 0) {
        s_sysclk_hz = BOARD_SYSCLK_HZ;
    }
    else {
        /* Supply commit or PLL step failed. Stay at HSI 64 MHz. The
         * UART BRR set in uart_program_brr at the top still matches
         * 64 MHz so console output continues to work. */
        board_puts("[H7A3] PLL bring-up failed; staying on HSI 64 MHz\n");
        s_sysclk_hz = HSI_DEFAULT_HZ;
    }

    board_common_systick_init(s_sysclk_hz);
}

uint32_t board_sysclk_hz(void)
{
    /* Reflects the actual outcome of clock_init_pll280 -- so the
     * SystemCoreClock-vs-board_sysclk_hz print in main_test.c
     * agrees when the chip is in the fallback HSI 64 MHz path. */
    return s_sysclk_hz;
}


const char *board_name(void)
{
    return "NUCLEO-H7A3ZI-Q";
}
