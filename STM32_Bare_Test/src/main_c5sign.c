/* main_c5sign.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * STM32C5A3 bare PKA PROTECTED ECDSA-sign reference probe.
 *
 * wolfSSL's HW PKA sign on the C5A3 completes (PROCENDF, OUT_ERROR=0xD60D OK)
 * but returns a wrong r,s. The C5 PKA implements ONLY the side-channel-
 * PROTECTED ECDSA modes (sign 0x24, verify 0x26). This probe replicates ST's
 * HAL_PKA_ECDSA_SetConfigSignatureProtect + HAL_PKA_Compute sequence directly,
 * driving the registers with ST's own NIST P-256 CAVP test vector (known d, k,
 * hash -> known R,S), then compares the HW output to ST's expected R,S.
 *
 * The C5 protected sign needs the RNG (ST's PKA init enables the RNG clock +
 * CK48<-HSIDIV3 and inits the RNG BEFORE the PKA). This probe brings the RNG
 * fully up first, exactly like ST. Pure register access -- no wolfCrypt.
 *
 *   PASS => the silicon + ST sequence are good; wolfSSL's driver differs.
 *   FAIL => the standalone protected sign itself does not reproduce -- the
 *           blocker is silicon state / RNG coupling, not wolfSSL's op order.
 */

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "stm32c5a3xx.h"
#include "st_p256_vec.h"

#ifndef WC_C5SIGN_TIMEOUT
#define WC_C5SIGN_TIMEOUT 0x10000000u
#endif

/* PKA RAM is uint8_t[] on C5; word-access it. */
#define PKA_RAMW ((volatile uint32_t*)(void*)PKA->RAM)

/* Optimal operand bit length, matching ST PKA_GetOptBitSize_u8 /
 * wolfSSL wc_stm32_pka_optbits: (bytes-1)*8 + bit-position-of-MSB. */
static uint32_t optbits(uint32_t nbyte, uint8_t msb)
{
    uint32_t pos = 0, v = msb;
    while (v != 0u) { v >>= 1; pos++; }
    if (nbyte == 0u) return 0u;
    return ((nbyte - 1u) * 8u) + pos;
}

/* Load a big-endian operand into PKA RAM as little-endian words (the layout
 * the PKA RAM expects, proven on U3/U5/N6). No explicit terminator: the PKA
 * RAM is erased (zeroed) at enable/INITOK, so the trailing zeros are present,
 * matching ST's SetConfig which writes operands into the erased RAM. */
static void load_be(uint32_t slot, const uint8_t* src, uint32_t n)
{
    volatile uint32_t* d = &PKA_RAMW[slot];
    uint32_t i = 0;
    for (; i < (n / 4u); i++) {
        d[i] = ((uint32_t)src[n - (i * 4u) - 1u])        |
               ((uint32_t)src[n - (i * 4u) - 2u] <<  8)  |
               ((uint32_t)src[n - (i * 4u) - 3u] << 16)  |
               ((uint32_t)src[n - (i * 4u) - 4u] << 24);
    }
}

static void read_be(uint8_t* dst, uint32_t slot, uint32_t n)
{
    volatile const uint32_t* s = &PKA_RAMW[slot];
    uint32_t i = 0;
    for (; i < (n / 4u); i++) {
        uint32_t off = n - 4u - (i * 4u);
        dst[off + 3u] = (uint8_t)( s[i]        & 0xFFu);
        dst[off + 2u] = (uint8_t)((s[i] >>  8) & 0xFFu);
        dst[off + 1u] = (uint8_t)((s[i] >> 16) & 0xFFu);
        dst[off + 0u] = (uint8_t)((s[i] >> 24) & 0xFFu);
    }
}

static int rng_bringup(void)
{
    uint32_t t, to = 2000000u;
    int condrst_cleared = 0, drdy = 0;

    RCC->CR1 |= RCC_CR1_HSIDIV3ON;
    while (((RCC->CR1 & RCC_CR1_HSIDIV3RDY) == 0u) && (--to != 0u)) { }
    RCC->CCIPR2 = (RCC->CCIPR2 & ~RCC_CCIPR2_CK48SEL_Msk) | RCC_CCIPR2_CK48SEL_1;
    (void)RCC->CCIPR2;
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    (void)RCC->AHB2ENR;

    RNG->CR &= ~RNG_CR_RNGEN;
    RNG->CR |= RNG_CR_CONDRST;
    RNG->CR = (uint32_t)RNG_CAND_NIST_CR_VALUE | (uint32_t)RNG_CR_CONDRST;
#ifdef RNG_CAND_NIST_HTCR_VALUE
    RNG->HTCR[0] = (uint32_t)RNG_CAND_NIST_HTCR_VALUE;
#endif
#ifdef RNG_CAND_NIST_NSCR_VALUE
    RNG->NSCR = (uint32_t)RNG_CAND_NIST_NSCR_VALUE;
#endif
    RNG->CR &= ~RNG_CR_CONDRST;
    for (t = 0; t < 2000000u; t++) {
        if ((RNG->CR & RNG_CR_CONDRST) == 0u) { condrst_cleared = 1; break; }
    }
    if (condrst_cleared) {
        RNG->CR |= RNG_CR_RNGEN;
        for (t = 0; t < 2000000u; t++) {
            if ((RNG->SR & RNG_SR_DRDY) != 0u) { drdy = 1; break; }
            if ((RNG->SR & RNG_SR_SEIS) != 0u) break;
        }
    }
    printf("RNG: condrst_cleared=%d drdy=%d SR=%08lx\n",
           condrst_cleared, drdy, (unsigned long)RNG->SR);
    return (condrst_cleared && drdy) ? 0 : -1;
}

/* V2 PARAM_END terminator (double zero word), like wolfSSL load_param_be. */
static void param_end(uint32_t slot, uint32_t bytes)
{
    uint32_t idx = slot + ((bytes + 3u) / 4u);
    PKA_RAMW[idx] = 0u;
    PKA_RAMW[idx + 1u] = 0u;
}

/* Protected sign. flags select which wolfSSL-isms to replicate:
 *   C5SIGN_TERMINATORS - write PARAM_END double-zeros after each operand.
 *   C5SIGN_REMODE      - re-write PKA_CR mode field AFTER operands (as
 *                        wc_stm32_pka_process does), before START.
 * The clean (flags==0) form matches ST's HAL order and is known to PASS. */
#define C5SIGN_TERMINATORS 0x1u
#define C5SIGN_REMODE      0x2u
static int protected_sign(uint8_t* rOut, uint8_t* sOut, uint32_t* srOut,
                          uint32_t flags)
{
    uint32_t t, cr;
    const uint32_t MODE_SIGN = 0x24u;

    /* enable + wait INITOK (PKA RAM erase complete) */
    if ((PKA->CR & PKA_CR_EN) == 0u) PKA->CR = PKA_CR_EN;
    for (t = 0; (PKA->SR & PKA_SR_INITOK) == 0u; t++) {
        if (t >= WC_C5SIGN_TIMEOUT) { printf("INITOK timeout\n"); return -1; }
    }

    /* set mode BEFORE loading operands (ST order) */
    cr = PKA->CR;
    cr &= ~PKA_CR_MODE;
    cr |= (MODE_SIGN << PKA_CR_MODE_Pos) & PKA_CR_MODE;
    PKA->CR = cr;
    __DMB();

    PKA_RAMW[PKA_ECDSA_SIGN_IN_ORDER_NB_BITS] = optbits(32, Prime256v1Order[0]);
    PKA_RAMW[PKA_ECDSA_SIGN_IN_MOD_NB_BITS]   = optbits(32, Prime256v1Prime[0]);
    PKA_RAMW[PKA_ECDSA_SIGN_IN_A_COEFF_SIGN]  = Prime256v1_A_Sign;
    load_be(PKA_ECDSA_SIGN_IN_A_COEFF,        Prime256v1AbsA, 32);
    load_be(PKA_ECDSA_SIGN_IN_B_COEFF,        Prime256v1_B, 32);
    load_be(PKA_ECDSA_SIGN_IN_MOD_GF,         Prime256v1Prime, 32);
    load_be(PKA_ECDSA_SIGN_IN_K,              SigGen_K, 32);
    load_be(PKA_ECDSA_SIGN_IN_INITIAL_POINT_X, Prime256v1GeneratorX, 32);
    load_be(PKA_ECDSA_SIGN_IN_INITIAL_POINT_Y, Prime256v1GeneratorY, 32);
    load_be(PKA_ECDSA_SIGN_IN_HASH_E,         SigGenHashMsg, 32);
    load_be(PKA_ECDSA_SIGN_IN_PRIVATE_KEY_D,  SigGen_D, 32);
    load_be(PKA_ECDSA_SIGN_IN_ORDER_N,        Prime256v1Order, 32);
    if (flags & C5SIGN_TERMINATORS) {
        param_end(PKA_ECDSA_SIGN_IN_A_COEFF, 32);
        param_end(PKA_ECDSA_SIGN_IN_B_COEFF, 32);
        param_end(PKA_ECDSA_SIGN_IN_MOD_GF, 32);
        param_end(PKA_ECDSA_SIGN_IN_K, 32);
        param_end(PKA_ECDSA_SIGN_IN_INITIAL_POINT_X, 32);
        param_end(PKA_ECDSA_SIGN_IN_INITIAL_POINT_Y, 32);
        param_end(PKA_ECDSA_SIGN_IN_HASH_E, 32);
        param_end(PKA_ECDSA_SIGN_IN_PRIVATE_KEY_D, 32);
        param_end(PKA_ECDSA_SIGN_IN_ORDER_N, 32);
    }
    __DMB();

    if (flags & C5SIGN_REMODE) {
        /* replicate wc_stm32_pka_process: re-write mode AFTER operands */
        cr = PKA->CR;
        cr &= ~PKA_CR_MODE;
        cr |= (MODE_SIGN << PKA_CR_MODE_Pos) & PKA_CR_MODE;
        PKA->CR = cr;
        __DMB();
    }

    /* clear stale flags, START, wait PROCENDF */
    PKA->CLRFR = PKA_CLRFR_PROCENDFC | PKA_CLRFR_RAMERRFC | PKA_CLRFR_ADDRERRFC;
#ifdef PKA_CLRFR_OPERRFC
    PKA->CLRFR = PKA_CLRFR_OPERRFC;
#endif
    __DMB();
    PKA->CR = cr | PKA_CR_START;
    __DMB();
    for (t = 0; (PKA->SR & PKA_SR_PROCENDF) == 0u; t++) {
        if (t >= WC_C5SIGN_TIMEOUT) { printf("PROCENDF timeout SR=%08lx\n",
            (unsigned long)PKA->SR); return -1; }
    }
    *srOut = PKA->SR;
    PKA->CLRFR = PKA_CLRFR_PROCENDFC;

    read_be(rOut, PKA_ECDSA_SIGN_OUT_SIGNATURE_R, 32);
    read_be(sOut, PKA_ECDSA_SIGN_OUT_SIGNATURE_S, 32);
    return 0;
}

/* ST-faithful protected verify (mode 0x26) of a known-good signature.
 * Returns the PKA_ECDSA_VERIF_OUT_RESULT code (0xD60D = valid on V2). */
static uint32_t protected_verify(uint32_t flags)
{
    uint32_t t, cr;
    const uint32_t MODE_VERIF = 0x26u;

    if ((PKA->CR & PKA_CR_EN) == 0u) PKA->CR = PKA_CR_EN;
    PKA->CR = 0u; __DMB();
    PKA->CR = PKA_CR_EN;
    for (t = 0; (PKA->SR & PKA_SR_INITOK) == 0u; t++) {
        if (t >= WC_C5SIGN_TIMEOUT) return 0xDEAD0001u;
    }
    cr = PKA->CR;
    cr &= ~PKA_CR_MODE;
    cr |= (MODE_VERIF << PKA_CR_MODE_Pos) & PKA_CR_MODE;
    PKA->CR = cr;
    __DMB();

    PKA_RAMW[PKA_ECDSA_VERIF_IN_ORDER_NB_BITS] = optbits(32, Prime256v1Order[0]);
    PKA_RAMW[PKA_ECDSA_VERIF_IN_MOD_NB_BITS]   = optbits(32, Prime256v1Prime[0]);
    PKA_RAMW[PKA_ECDSA_VERIF_IN_A_COEFF_SIGN]  = Prime256v1_A_Sign;
    load_be(PKA_ECDSA_VERIF_IN_A_COEFF,            Prime256v1AbsA, 32);
    load_be(PKA_ECDSA_VERIF_IN_MOD_GF,             Prime256v1Prime, 32);
    load_be(PKA_ECDSA_VERIF_IN_INITIAL_POINT_X,    Prime256v1GeneratorX, 32);
    load_be(PKA_ECDSA_VERIF_IN_INITIAL_POINT_Y,    Prime256v1GeneratorY, 32);
    load_be(PKA_ECDSA_VERIF_IN_PUBLIC_KEY_POINT_X, SigGen_Qx, 32);
    load_be(PKA_ECDSA_VERIF_IN_PUBLIC_KEY_POINT_Y, SigGen_Qy, 32);
    load_be(PKA_ECDSA_VERIF_IN_SIGNATURE_R,        SigGen_R, 32);
    load_be(PKA_ECDSA_VERIF_IN_SIGNATURE_S,        SigGen_S, 32);
    load_be(PKA_ECDSA_VERIF_IN_HASH_E,             SigGenHashMsg, 32);
    load_be(PKA_ECDSA_VERIF_IN_ORDER_N,            Prime256v1Order, 32);
    if (flags & C5SIGN_TERMINATORS) {
        param_end(PKA_ECDSA_VERIF_IN_A_COEFF, 32);
        param_end(PKA_ECDSA_VERIF_IN_MOD_GF, 32);
        param_end(PKA_ECDSA_VERIF_IN_INITIAL_POINT_X, 32);
        param_end(PKA_ECDSA_VERIF_IN_INITIAL_POINT_Y, 32);
        param_end(PKA_ECDSA_VERIF_IN_PUBLIC_KEY_POINT_X, 32);
        param_end(PKA_ECDSA_VERIF_IN_PUBLIC_KEY_POINT_Y, 32);
        param_end(PKA_ECDSA_VERIF_IN_SIGNATURE_R, 32);
        param_end(PKA_ECDSA_VERIF_IN_SIGNATURE_S, 32);
        param_end(PKA_ECDSA_VERIF_IN_HASH_E, 32);
        param_end(PKA_ECDSA_VERIF_IN_ORDER_N, 32);
    }
    __DMB();

    if (flags & C5SIGN_REMODE) {
        cr = PKA->CR;
        cr &= ~PKA_CR_MODE;
        cr |= (MODE_VERIF << PKA_CR_MODE_Pos) & PKA_CR_MODE;
        PKA->CR = cr;
        __DMB();
    }

    PKA->CLRFR = PKA_CLRFR_PROCENDFC | PKA_CLRFR_RAMERRFC | PKA_CLRFR_ADDRERRFC;
#ifdef PKA_CLRFR_OPERRFC
    PKA->CLRFR = PKA_CLRFR_OPERRFC;
#endif
    __DMB();
    PKA->CR = cr | PKA_CR_START;
    __DMB();
    for (t = 0; (PKA->SR & PKA_SR_PROCENDF) == 0u; t++) {
        if (t >= WC_C5SIGN_TIMEOUT) return 0xDEAD0002u;
    }
    PKA->CLRFR = PKA_CLRFR_PROCENDFC;
    return PKA_RAMW[PKA_ECDSA_VERIF_OUT_RESULT];
}

static void dump(const char* tag, const uint8_t* p)
{
    int i;
    printf("  %s = ", tag);
    for (i = 0; i < 32; i++) printf("%02x", p[i]);
    printf("\n");
}

int main(void)
{
    uint8_t r[32], s[32];
    uint32_t sr = 0;
    int rc;

    board_init();
    printf("\n========================================\n");
    printf("STM32C5A3 bare PROTECTED ECDSA sign vs ST P-256 CAVP vector\n");
    printf("========================================\n\n");

    (void)rng_bringup();          /* RNG up first, like ST */
    RCC->AHB2ENR |= RCC_AHB2ENR_PKAEN;
    (void)RCC->AHB2ENR;

    {
        struct { const char* name; uint32_t flags; } variants[] = {
            { "clean (ST order)            ", 0u },
            { "+terminators               ", C5SIGN_TERMINATORS },
            { "+remode (post-operand mode) ", C5SIGN_REMODE },
            { "+terminators +remode (wolf) ", C5SIGN_TERMINATORS | C5SIGN_REMODE },
        };
        int v, npass = 0;
        for (v = 0; v < 4; v++) {
            int ok;
            rc = protected_sign(r, s, &sr, variants[v].flags);
            ok = (rc == 0 && memcmp(r, SigGen_R, 32) == 0 &&
                  memcmp(s, SigGen_S, 32) == 0);
            printf("[%s] rc=%d SR=%08lx err=%lx -> %s\n",
                   variants[v].name, rc, (unsigned long)sr,
                   (unsigned long)PKA_RAMW[PKA_ECDSA_SIGN_OUT_ERROR],
                   ok ? "PASS" : "FAIL");
            if (!ok) { dump("  R   ", r); dump("  Rexp", SigGen_R); }
            if (ok) npass++;
        }
        printf("\nResult: %d (%d/4 variants PASS)\n", (npass == 4) ? 0 : -1,
               npass);
    }

    /* RNG-dependency probe: does the protected sign read RNG_DR for blinding?
     * Disable the RNG, then sign with the known-good clean sequence. */
    {
        int ok;
        RNG->CR &= ~RNG_CR_RNGEN;
        __DMB();
        rc = protected_sign(r, s, &sr, 0u);
        ok = (rc == 0 && memcmp(r, SigGen_R, 32) == 0 &&
              memcmp(s, SigGen_S, 32) == 0);
        printf("[RNG DISABLED (RNGEN=0)] rc=%d SR=%08lx -> %s\n", rc,
               (unsigned long)sr, ok ? "PASS (RNG not needed)" : "FAIL (RNG needed!)");
        if (!ok) { dump("  R   ", r); dump("  Rexp", SigGen_R); }
    }

    /* Protected VERIFY of ST's known-good R,S (mode 0x26), RNG OFF (current). */
    {
        uint32_t vres = protected_verify(0u);
        printf("[VERIFY clean, RNG OFF] OUT_RESULT=%08lx -> %s\n",
               (unsigned long)vres,
               (vres == 0xD60Du) ? "VALID" : "INVALID/err");
    }
    /* Now RE-ENABLE + condition the RNG (wolfSSL keeps the HW RNG running) and
     * verify again -- isolates whether an active RNG breaks the protected
     * verify (the one contextual diff vs the failing wolfSSL path). */
    {
        uint32_t vres;
        (void)rng_bringup();
        vres = protected_verify(0u);
        printf("[VERIFY clean, RNG ON ] OUT_RESULT=%08lx -> %s\n",
               (unsigned long)vres,
               (vres == 0xD60Du) ? "VALID" : "INVALID/err");
    }
    printf("Test complete\n");
    for (;;) { }
}
