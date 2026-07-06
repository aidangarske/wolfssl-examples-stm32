/* main_stsaes.c - run ST's exact HAL SAES wrapped-key (DHUK) flow on the
 * attached board, mirroring Projects/B-U585I-IOT02A/Examples/CRYP/
 * CRYP_SAES_WrapKey. Calls the ST HAL directly (HAL_CRYP_Init +
 * HAL_CRYPEx_WrapKey/UnwrapKey + HAL_CRYP_Encrypt/Decrypt) so we can tell
 * whether the HAL DHUK wrap/unwrap completes on this silicon vs our bare
 * driver. Results land in g_st_saes (read via openocd / CubeProgrammer):
 *   magic=0x57AE0001 once main reaches the end.
 *
 * Copyright (C) 2026 wolfSSL Inc.
 */
#include "stm32u5xx_hal.h"
#include <stdint.h>
#include <string.h>
#include "board.h"

CRYP_HandleTypeDef hcryp;
RNG_HandleTypeDef  hrng;

/* Debugger-readable result sink. */
volatile struct {
    uint32_t magic;
    int32_t  shsi_rc;     /* HAL_RCC_OscConfig (SHSI ON)            */
    int32_t  saesclk_rc;  /* HAL_RCCEx_PeriphCLKConfig (SAES=SHSI)  */
    int32_t  rng_init_rc;
    int32_t  cryp_init_rc;
    int32_t  wrap_rc;     /* HAL_CRYPEx_WrapKey   status (0=HAL_OK) */
    int32_t  unwrap_rc;   /* HAL_CRYPEx_UnwrapKey status (0=HAL_OK) */
    int32_t  enc_rc;      /* HAL_CRYP_Encrypt     status            */
    int32_t  dec_rc;      /* HAL_CRYP_Decrypt     status            */
    int32_t  roundtrip;   /* 0 = decrypt(encrypt(pt)) == pt         */
    uint32_t saes_sr;     /* SAES SR after the sequence             */
    uint32_t saes_cr;     /* SAES CR after the sequence             */
} g_st_saes;

/* A 256-bit key to wrap (arbitrary -- the wrap/unwrap round-trips it). */
static uint32_t Key256[8] = {
    0x603DEB10u, 0x15CA71BEu, 0x2B73AEF0u, 0x857D7781u,
    0x1F352C07u, 0x3B6108D7u, 0x2D9810A3u, 0x0914DFF4u
};
static uint32_t Wrapped[8];
static uint32_t Plaintext[4]  = { 0x6BC1BEE2u, 0x2E409F96u, 0xE93D7E11u, 0x7393172Au };
static uint32_t CipherText[4];
static uint32_t RoundTrip[4];

#define TIMEOUT_VALUE 0xFFFFU

int main(void)
{
    g_st_saes.magic = 0u;

    board_init();        /* clock + SysTick (HAL_GetTick) + putc */

    /* Kernel clocks the SAES self-init / DHUK need. The SAES on U5 runs from
     * the SHSI (secure HSI) -- it must be ON and selected as the SAES kernel
     * clock or the IP never computes (CCF never fires). Mirrors the example's
     * SystemClock_Config (SHSI ON) + HAL_CRYP_MspInit (SaesClockSelection). */
    {
        RCC_OscInitTypeDef osc = {0};
        RCC_PeriphCLKInitTypeDef periph = {0};
        osc.OscillatorType = RCC_OSCILLATORTYPE_SHSI;
        osc.SHSIState = RCC_SHSI_ON;
        g_st_saes.shsi_rc = (int32_t)HAL_RCC_OscConfig(&osc);
        periph.PeriphClockSelection = RCC_PERIPHCLK_SAES;
        periph.SaesClockSelection = RCC_SAESCLKSOURCE_SHSI;
        g_st_saes.saesclk_rc = (int32_t)HAL_RCCEx_PeriphCLKConfig(&periph);
    }
    __HAL_RCC_RNG_CLK_ENABLE();
    __HAL_RCC_SAES_CLK_ENABLE();
    HAL_Delay(10);

    /* RNG (SAES self-init pulls entropy). */
    hrng.Instance = RNG;
    g_st_saes.rng_init_rc = (int32_t)HAL_RNG_Init(&hrng);

    /* SAES config -- exactly the example's MX_SAES_AES_Init. */
    hcryp.Instance        = SAES;
    hcryp.Init.DataType   = CRYP_NO_SWAP;
    hcryp.Init.KeySize    = CRYP_KEYSIZE_256B;
    hcryp.Init.Algorithm  = CRYP_AES_ECB;
    hcryp.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_WORD;
    hcryp.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ALWAYS;
    hcryp.Init.KeyMode    = CRYP_KEYMODE_WRAPPED;
    hcryp.Init.KeySelect  = CRYP_KEYSEL_HW;       /* DHUK */
    hcryp.Init.KeyProtection = CRYP_KEYPROT_DISABLE;
    g_st_saes.cryp_init_rc = (int32_t)HAL_CRYP_Init(&hcryp);

    /* Wrap the key under DHUK, then unwrap it back into KEYR. */
    g_st_saes.wrap_rc   = (int32_t)HAL_CRYPEx_WrapKey(&hcryp, Key256, Wrapped,
                                                      TIMEOUT_VALUE);
    g_st_saes.unwrap_rc = (int32_t)HAL_CRYPEx_UnwrapKey(&hcryp, Wrapped,
                                                        TIMEOUT_VALUE);

    /* Use the unwrapped key: encrypt then decrypt one block, check round-trip. */
    g_st_saes.enc_rc = (int32_t)HAL_CRYP_Encrypt(&hcryp, Plaintext, 4, CipherText,
                                                 TIMEOUT_VALUE);
    g_st_saes.dec_rc = (int32_t)HAL_CRYP_Decrypt(&hcryp, CipherText, 4, RoundTrip,
                                                 TIMEOUT_VALUE);
    g_st_saes.roundtrip = (memcmp(RoundTrip, Plaintext, sizeof(Plaintext)) == 0)
                              ? 0 : -1;

    g_st_saes.saes_sr = SAES->SR;
    g_st_saes.saes_cr = SAES->CR;
    g_st_saes.magic   = 0x57AE0001u;

    for (;;) { }
}

/* HAL tick is driven by board_common's SysTick handler -> HAL_IncTick. */
