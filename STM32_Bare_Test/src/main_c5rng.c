/* main_c5rng.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * STM32C5A3 HW-RNG conditioning probe (CCB smoke test, RNG leg).
 *
 * The CCB-protected ECDSA path needs the HW RNG (the CCB chains RNG -> PKA for
 * the random k / blob IV; ST's CCB_RNG_Init uses the standard CONDRST + NIST
 * conditioning). On the C5A3 the HW RNG is held off today (NO_STM32_RNG, SW
 * Hash-DRBG) because conditioning does not complete. This probe runs the exact
 * CCB_RNG_Init sequence on the live silicon and reports the precise failure mode
 * (CECS clock-error vs CONDRST-stuck vs no-DRDY), to settle whether CCB on C5A3
 * is blocked by the RNG. Pure register access -- no wolfCrypt.
 */

#include <stdio.h>

#include "board.h"
#include "stm32c5a3xx.h"

#ifndef WC_C5RNG_TIMEOUT
#define WC_C5RNG_TIMEOUT 2000000u
#endif

static void dump_sr(const char* tag)
{
    uint32_t sr = RNG->SR;
    printf("  %-14s CR=%08lx SR=%08lx  CECS=%lu CEIS=%lu SECS=%lu SEIS=%lu DRDY=%lu\n",
           tag, (unsigned long)RNG->CR, (unsigned long)sr,
           (unsigned long)((sr & RNG_SR_CECS) != 0u),
           (unsigned long)((sr & RNG_SR_CEIS) != 0u),
           (unsigned long)((sr & RNG_SR_SECS) != 0u),
           (unsigned long)((sr & RNG_SR_SEIS) != 0u),
           (unsigned long)((sr & RNG_SR_DRDY) != 0u));
}

int main(void)
{
    uint32_t t;
    int condrst_cleared = 0;
    int drdy = 0;

    board_init();

    printf("\n========================================\n");
    printf("STM32C5A3 HW-RNG conditioning probe - %s\n", board_name());
    printf("(CCB_RNG_Init sequence; CLKDIV in NIST CR = %lu)\n",
           (unsigned long)((RNG_CAND_NIST_CR_VALUE & RNG_CR_CLKDIV_Msk)
                           >> RNG_CR_CLKDIV_Pos));
    printf("========================================\n\n");

    /* CK48 is the RNG kernel clock; its mux (CCIPR2.CK48SEL) defaults to NONE,
     * so the RNG has no kernel clock. Enable HSIDIV3 (HSI/3 = 48 MHz, internal,
     * no crystal dependency) and select it as the CK48 source. */
    {
        uint32_t to = WC_C5RNG_TIMEOUT;
        RCC->CR1 |= RCC_CR1_HSIDIV3ON;
        while (((RCC->CR1 & RCC_CR1_HSIDIV3RDY) == 0u) && (--to != 0u)) { }
        printf("HSIDIV3RDY: %s\n",
               (RCC->CR1 & RCC_CR1_HSIDIV3RDY) ? "YES" : "NO");
    }
    printf("CCIPR2 (pre)  = %08lx\n", (unsigned long)RCC->CCIPR2);
    RCC->CCIPR2 = (RCC->CCIPR2 & ~RCC_CCIPR2_CK48SEL_Msk)
                  | RCC_CCIPR2_CK48SEL_1;  /* HSIDIV3 */
    (void)RCC->CCIPR2;
    printf("CCIPR2 (post) = %08lx (CK48SEL=HSIDIV3)\n", (unsigned long)RCC->CCIPR2);

    /* RNG bus clock. */
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    (void)RCC->AHB2ENR;

    printf("RNG kernel: RCC->CR1=%08lx CR2=%08lx (HSIKON/HSIKRDY/HSIKDIV)\n",
           (unsigned long)RCC->CR1, (unsigned long)RCC->CR2);
    dump_sr("at-entry");

    /* --- ST CCB_RNG_Init sequence --- */
    RNG->CR &= ~RNG_CR_RNGEN;                 /* disable */
    RNG->CR |= RNG_CR_CONDRST;                /* enter conditioning reset */
    RNG->CR = (uint32_t)RNG_CAND_NIST_CR_VALUE | (uint32_t)RNG_CR_CONDRST;
#ifdef RNG_CAND_NIST_HTCR_VALUE
    RNG->HTCR[0] = (uint32_t)RNG_CAND_NIST_HTCR_VALUE;
#endif
#ifdef RNG_CAND_NIST_NSCR_VALUE
    RNG->NSCR = (uint32_t)RNG_CAND_NIST_NSCR_VALUE;
#endif
    dump_sr("nist-written");

    RNG->CR &= ~RNG_CR_CONDRST;               /* latch config */
    for (t = 0; t < WC_C5RNG_TIMEOUT; t++) {
        if ((RNG->CR & RNG_CR_CONDRST) == 0u) { condrst_cleared = 1; break; }
    }
    printf("CONDRST cleared: %s (after %lu iters)\n",
           condrst_cleared ? "YES" : "NO (STUCK)", (unsigned long)t);
    dump_sr("post-condrst");

    if (condrst_cleared) {
        RNG->CR |= RNG_CR_RNGEN;              /* enable */
        for (t = 0; t < WC_C5RNG_TIMEOUT; t++) {
            if ((RNG->SR & RNG_SR_DRDY) != 0u) { drdy = 1; break; }
            if ((RNG->SR & RNG_SR_SEIS) != 0u) { break; }   /* seed error */
        }
        printf("DRDY: %s (after %lu iters)\n",
               drdy ? "YES" : "NO", (unsigned long)t);
        dump_sr("post-enable");
        if (drdy) {
            printf("RNG_DR samples: %08lx %08lx\n",
                   (unsigned long)RNG->DR, (unsigned long)RNG->DR);
        }
    }

    printf("\nResult: %s\n",
           (condrst_cleared && drdy) ? "0 (RNG conditioned -- CCB RNG viable)"
                                     : "-1 (RNG blocked -- CCB ECDSA cannot run)");
    printf("Test complete\n");
    for (;;) { }
}
