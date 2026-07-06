/* hw_init.c - STM32H573ZI (NUCLEO-H573ZI), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-H573ZI:
 *   - HSI 64 MHz as SYSCLK (no PLL); flash latency 1WS at default VOS
 *   - USART3 on PD8 (TX) / PD9 (RX) AF7, 115200 8N1, routed to ST-LINK VCP
 *   - Crypto: AES + HASH + RNG + SAES + PKA + DHUK (full set, vs H563
 *     which has only HASH + RNG + PKA). The third TZ=0 DHUK silicon
 *     in the matrix (alongside U385 + WBA52) -- useful for chasing
 *     the DHUK unwrap-decrypt mystery.
 *
 * No HAL drivers are pulled in. All state lives directly in MMIO.
 *
 * Status (2026-05-06): TZEN=0xC3 set on the lab board. Reset+halt+inspect
 * confirms the chip enters the M33 NMI handler at every reset
 * (xPSR.exception=2, mode = Handler NMI). User confirms wolfIP and
 * wolfBoot bare-metal builds run fine on this same chip, so the silicon
 * is healthy -- the issue is in our build/flash flow, not the board.
 *
 * Flash content readback at the suspect address matches our binary
 * exactly, and FLASH NSSR / ECCR registers show no ECC error flag
 * pending at halt time (so the original "ECC fault" diagnosis was
 * wrong; NMI source is something else). The minimal no-op NMI handler
 * below avoids the lockup that previously masked the issue.
 *
 * Decisive isolation experiment (2026-05-06): a minimal binary
 * (vector table at 0x08000000 + Reset_Handler that writes a marker
 * 0xCAFEF00D to 0x20000000 then spins) flashes via the same OpenOCD
 * + ST-fork command, runs clean -- PC stops in the spin loop, the
 * RAM markers verify, no NMI fires. So the H5 chip is healthy and
 * our flash flow is fine.
 *
 * The NMI fires only with our larger wolfssl-bare-metal binary --
 * tried both the ST asm startup_stm32h563xx.s and the wolfIP-pattern
 * C startup (boards/h5/{ivt.c,startup.c}, since deleted). Same
 * NMI->lockup with both. So the trigger is not the startup itself
 * but something in the rest of the binary -- candidates: (a) newlib
 * __libc_init_array touching a peripheral that fires NMI, (b) the
 * .data section content / size when .data init reads from a flash
 * region the chip doesn't like, (c) a stray write to a clock-source
 * security register during wolfssl init.
 *
 * Tracked as the next H5 follow-up: bisect by gradually adding
 * functions back from the minimal binary until NMI re-appears.
 */

#include "stm32h5xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- NMI handler --------------------------------------------------------
 * The lab H5 has a flash cell that fires an ECC detection error during
 * instruction fetch (FLASH_ECCR.ECCD bit), which on the H5 family is
 * routed to NMI unconditionally. The startup file's default NMI handler
 * is an infinite loop -- that's what was locking the chip up post-reset
 * with no UART output. Override the NMI handler so it write-1-clears
 * the ECC flags and returns; the offending fetch may then complete via
 * ECC correction (single-bit) or refire NMI (uncorrectable, in which
 * case we still loop, but at least the chip doesn't lock up entirely
 * before that point). This is a workaround for board-side flash damage;
 * the proper fix is a fresh chip.
 *
 * The H5 FLASH_ECCR sits at offset 0x1C of the FLASH peripheral block
 * (between OPSR at 0x18 and NSSR at 0x20). The CMSIS FLASH_TypeDef
 * doesn't expose it as a struct member, so access it directly. */
#define WC_H5_FLASH_ECCR_NS  (*(volatile uint32_t *)(FLASH_R_BASE_NS + 0x1Cu))
#define WC_H5_FLASH_ECCD     (1u << 31)
#define WC_H5_FLASH_ECCC     (1u << 30)

void NMI_Handler(void)
{
    /* No-op return -- writing FLASH peripheral registers without first
     * unlocking via NSKEYR causes a bus fault, which inside an NMI
     * escalates to lockup. A bare NMI return at least lets us see if
     * the ECC error self-clears or if it persists (flash damage). */
    __DSB();
    __ISB();
}

/* ---- printf retarget over USART3 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART3->ISR & USART_ISR_TXE_TXFNF) == 0) {
        /* wait for TX FIFO space */
    }
    USART3->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
static void clock_init(void)
{
    /* HSI is on by default at reset (64 MHz). Keep it as SYSCLK.
     * Set 1 wait state for flash at 64 MHz, default VOS. */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) | (1u << FLASH_ACR_LATENCY_Pos);

    /* AHB / APB prescalers = 1 (default); SYSCLK source = HSI (default).
     * Nothing more to do unless we want a higher SYSCLK (HSI -> PLL would
     * push to 250 MHz, but 64 MHz is plenty for HASH/RNG bring-up). */

    /* Enable HSI48 -- required as the RNG kernel clock on H5. Without it,
     * the RNG never produces DRDY and wc_GenerateSeed spins forever. */
    RCC->CR |= RCC_CR_HSI48ON;
    while ((RCC->CR & RCC_CR_HSI48RDY) == 0u) {
        /* spin until ready */
    }
}

/* ---- USART3 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* GPIOD clock (AHB2 on H5) + USART3 clock (APB1 low). */
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIODEN;
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;
    (void)RCC->APB1LENR;

    /* PD8 (TX) / PD9 (RX) AF7, USART3 at PCLK1 = 64 MHz. */
    board_common_uart_pin_init(GPIOD, 8u, 9u, 7u);
    board_common_uart_basic_init(USART3, 64000000u, 115200u);
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* Force-enable FPU CP10/CP11 full access. The CMSIS SystemInit only
     * does this if __FPU_USED is set at compile time, and the gating in
     * the H5 template can leave it disabled even with -mfloat-abi=hard. */
    SCB->CPACR |= (0xFu << 20);
    /* Disable FPU lazy stacking (LSPEN=0). With LSPEN, exception entry
     * defers FPU register push -- if the deferred push later fails (e.g.
     * stack underflow at a precise address), we get LSPERR which
     * escalates to HardFault. Force eager push instead. */
    FPU->FPCCR &= ~(FPU_FPCCR_LSPEN_Msk);
    __DSB();
    __ISB();

    /* Disable H5 ICACHE -- boot ROM may leave it in an inconsistent state
     * that returns 0xFFFFFFFF for some flash reads. Can be re-enabled later
     * after invalidate. */
    if ((ICACHE->CR & ICACHE_CR_EN) != 0u) {
        ICACHE->CR &= ~ICACHE_CR_EN;
        __DSB();
        __ISB();
    }

    SystemInit();   /* CMSIS template -- vector table + default clocks */
    clock_init();
    uart_init();
    board_common_systick_init(64000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 64000000u;
}


const char *board_name(void)
{
    return "NUCLEO-H573ZI";
}
