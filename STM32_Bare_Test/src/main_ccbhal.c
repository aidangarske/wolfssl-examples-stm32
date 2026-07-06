/* main_ccbhal.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * ST HAL CCB (Coupling and Chaining Bridge) ECDSA reference for STM32U385.
 * Build: make BOARD=u3 BUILD=cubemx TARGET=ccbhal
 *
 * Purpose: confirm the CCB-protected ECDSA flow works on the U385 silicon from
 * non-secure state, and produce a known-good reference (blob + public key +
 * signature) for the bare WOLFSSL_STM32_CCB port to be validated against. Uses
 * ST's HAL CCB driver directly; the DHUK is the blob encryption key
 * (HAL_CCB_USER_KEY_HW), so the P-256 private scalar never enters software.
 *
 *   [1] HAL_CCB_ECDSA_WrapPrivateKey  -- create the ECDSA signature blob from a
 *       known P-256 d (self-verified internally per RM0487).
 *   [2] HAL_CCB_ECDSA_ComputePublicKey -- derive the public key from the blob.
 *   [3] HAL_CCB_ECDSA_Sign            -- sign a fixed hash using the blob.
 *   [4] wolfSSL software verify of (r,s) against the HAL-computed public key.
 *
 * P-256 curve vectors are NIST values (matching ST's CCB example set).
 */

#include <stdio.h>
#include <string.h>

#include "stm32u3xx_hal.h"
#include "board.h"

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/version.h"
#include "wolfssl/wolfcrypt/types.h"
#include "wolfssl/wolfcrypt/wc_port.h"
#include "wolfssl/wolfcrypt/error-crypt.h"
#include "wolfssl/wolfcrypt/ecc.h"
#include "wolfssl/wolfcrypt/integer.h"

#ifndef BUILD_CONFIG_NAME
#define BUILD_CONFIG_NAME "unknown"
#endif

/* ---- NIST P-256 curve parameters (big-endian, 32 bytes) ---------------- */
static const uint8_t p256_d[32] = {
    0x51,0x9b,0x42,0x3d,0x71,0x5f,0x8b,0x58,0x1f,0x4f,0xa8,0xee,0x59,0xf4,0x77,0x1a,
    0x5b,0x44,0xc8,0x13,0x0b,0x4e,0x3e,0xac,0xca,0x54,0xa5,0x6d,0xda,0x72,0xb4,0x64};
static const uint8_t p256_a_abs[32] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x03};
static const uint8_t p256_b[32] = {
    0x5a,0xc6,0x35,0xd8,0xaa,0x3a,0x93,0xe7,0xb3,0xeb,0xbd,0x55,0x76,0x98,0x86,0xbc,
    0x65,0x1d,0x06,0xb0,0xcc,0x53,0xb0,0xf6,0x3b,0xce,0x3c,0x3e,0x27,0xd2,0x60,0x4b};
static const uint8_t p256_p[32] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
static const uint8_t p256_n[32] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xbc,0xe6,0xfa,0xad,0xa7,0x17,0x9e,0x84,0xf3,0xb9,0xca,0xc2,0xfc,0x63,0x25,0x51};
static const uint8_t p256_Gx[32] = {
    0x6b,0x17,0xd1,0xf2,0xe1,0x2c,0x42,0x47,0xf8,0xbc,0xe6,0xe5,0x63,0xa4,0x40,0xf2,
    0x77,0x03,0x7d,0x81,0x2d,0xeb,0x33,0xa0,0xf4,0xa1,0x39,0x45,0xd8,0x98,0xc2,0x96};
static const uint8_t p256_Gy[32] = {
    0x4f,0xe3,0x42,0xe2,0xfe,0x1a,0x7f,0x9b,0x8e,0xe7,0xeb,0x4a,0x7c,0x0f,0x9e,0x16,
    0x2b,0xce,0x33,0x57,0x6b,0x31,0x5e,0xce,0xcb,0xb6,0x40,0x68,0x37,0xbf,0x51,0xf5};

/* Fixed message hash to sign (arbitrary 32-byte test value). */
static const uint8_t test_hash[32] = {
    0x44,0xac,0xf6,0xb7,0xe3,0x6c,0x13,0x42,0xc2,0xc5,0x89,0x72,0x04,0xfe,0x09,0x50,
    0x4e,0x1e,0x2e,0xfb,0x1a,0x90,0x03,0x77,0xdb,0xc4,0xe7,0xa6,0xa1,0x33,0xec,0x56};

/* HAL CCB working buffers. */
static CCB_HandleTypeDef        hccb;
static CCB_ECDSACurveParamTypeDef ccbParam;
static CCB_WrappingKeyTypeDef     wrapConf;
static CCB_ECDSAKeyBlobTypeDef    ccbBlob;
static CCB_ECDSASignTypeDef       ccbSig;
static CCB_ECCMulPointTypeDef     pubOut;

static uint32_t blob_iv[4];
static uint32_t blob_tag[4];
static uint32_t blob_wrapped[8];
static uint8_t  pub_x[32];
static uint8_t  pub_y[32];
static uint8_t  sig_r[32];
static uint8_t  sig_s[32];

/* CCB MspInit: enable the clocks for every peer the CCB chains (RM0487
 * 31.5.1). Called by HAL_CCB_Init(). */
void HAL_CCB_MspInit(CCB_HandleTypeDef *h)
{
    if (h->Instance == CCB) {
        __HAL_RCC_CCB_CLK_ENABLE();
        __HAL_RCC_PKA_CLK_ENABLE();
        __HAL_RCC_SAES_CLK_ENABLE();
        __HAL_RCC_RNG_CLK_ENABLE();
    }
}

static void print_hex(const char *label, const uint8_t *p, uint32_t n)
{
    uint32_t i;
    printf("  %s:", label);
    for (i = 0; i < n; i++) {
        printf(" %02x", p[i]);
    }
    printf("\n");
}

/* Verify (r,s) over test_hash with the HAL-computed public key, in software. */
static int sw_verify(void)
{
    ecc_key key;
    mp_int  r, s;
    int     ret;
    int     verified = 0;

    ret = wc_ecc_init(&key);
    if (ret != 0) {
        return ret;
    }
    ret = wc_ecc_import_unsigned(&key, pub_x, pub_y, NULL, ECC_SECP256R1);
    if (ret == 0) {
        ret = mp_init_multi(&r, &s, NULL, NULL, NULL, NULL);
    }
    if (ret == 0) {
        ret = mp_read_unsigned_bin(&r, sig_r, sizeof(sig_r));
        if (ret == 0) {
            ret = mp_read_unsigned_bin(&s, sig_s, sizeof(sig_s));
        }
        if (ret == 0) {
            ret = wc_ecc_verify_hash_ex(&r, &s, test_hash, sizeof(test_hash),
                                        &verified, &key);
        }
        mp_clear(&r);
        mp_clear(&s);
    }
    wc_ecc_free(&key);
    if (ret != 0) {
        return ret;
    }
    return verified ? 0 : -1;
}

int main(void)
{
    int ret = 0;
    HAL_StatusTypeDef hs;

    board_init();

    printf("\n========================================\n");
    printf("ST HAL CCB ECDSA reference - %s (CONFIG=%s)\n",
           board_name(), BUILD_CONFIG_NAME);
    printf("wolfSSL version: %s\n", LIBWOLFSSL_VERSION_STRING);
    printf("========================================\n\n");

    ret = wolfCrypt_Init();
    if (ret != 0) {
        printf("wolfCrypt_Init failed: %d\n", ret);
        for (;;) { }
    }

    hccb.Instance = CCB;
    hs = HAL_CCB_Init(&hccb);
    printf("[0] HAL_CCB_Init: %d (state=%lu)\n", (int)hs,
           (unsigned long)hccb.State);
    if (hs != HAL_OK) {
        ret = -1;
        goto done;
    }

    wrapConf.WrappingKeyType = HAL_CCB_USER_KEY_HW; /* DHUK is the blob key */

    ccbParam.primeOrderSizeByte = 32;
    ccbParam.modulusSizeByte    = 32;
    ccbParam.coefSignA          = 0x00000001u;
    ccbParam.pAbsCoefA          = p256_a_abs;
    ccbParam.pCoefB             = p256_b;
    ccbParam.pModulus           = p256_p;
    ccbParam.pPrimeOrder        = p256_n;
    ccbParam.pPointX            = p256_Gx;
    ccbParam.pPointY            = p256_Gy;

    ccbBlob.pIV         = blob_iv;
    ccbBlob.pTag        = blob_tag;
    ccbBlob.pWrappedKey = blob_wrapped;

    printf("[1] HAL_CCB_ECDSA_WrapPrivateKey (create + self-verify blob):\n");
    hs = HAL_CCB_ECDSA_WrapPrivateKey(&hccb, &ccbParam, p256_d, &wrapConf,
                                      &ccbBlob);
    printf("    ret=%d state=%lu operr=0x%lX\n", (int)hs,
           (unsigned long)hccb.State,
           (unsigned long)HAL_CCB_GetOperationError(&hccb));
    if ((hs != HAL_OK) || (hccb.State != HAL_CCB_STATE_READY)) {
        ret = -2;
        goto done;
    }
    print_hex("blob IV ", (const uint8_t *)blob_iv, sizeof(blob_iv));
    print_hex("blob tag", (const uint8_t *)blob_tag, sizeof(blob_tag));
    print_hex("wrapped d", (const uint8_t *)blob_wrapped, sizeof(blob_wrapped));

    printf("[2] HAL_CCB_ECDSA_ComputePublicKey:\n");
    pubOut.pPointX = pub_x;
    pubOut.pPointY = pub_y;
    hs = HAL_CCB_ECDSA_ComputePublicKey(&hccb, &ccbParam, &wrapConf, &ccbBlob,
                                        &pubOut);
    printf("    ret=%d\n", (int)hs);
    if (hs != HAL_OK) {
        ret = -3;
        goto done;
    }
    print_hex("pub X", pub_x, sizeof(pub_x));
    print_hex("pub Y", pub_y, sizeof(pub_y));

    printf("[3] HAL_CCB_ECDSA_Sign:\n");
    ccbSig.pRSign = sig_r;
    ccbSig.pSSign = sig_s;
    hs = HAL_CCB_ECDSA_Sign(&hccb, &ccbParam, &wrapConf, &ccbBlob,
                            (uint8_t *)test_hash, &ccbSig);
    printf("    ret=%d\n", (int)hs);
    if (hs != HAL_OK) {
        ret = -4;
        goto done;
    }
    print_hex("sig r", sig_r, sizeof(sig_r));
    print_hex("sig s", sig_s, sizeof(sig_s));

    printf("[4] wolfSSL software verify of (r,s) vs CCB public key:\n");
    ret = sw_verify();
    printf("    verify ret=%d (%s)\n", ret, ret == 0 ? "VALID" : "INVALID");

done:
    (void)HAL_CCB_DeInit(&hccb);
    wolfCrypt_Cleanup();
    printf("\nResult: %d (%s)\n", ret, ret == 0 ? "PASS" : "FAIL");
    printf("Test complete\n");
    for (;;) { }
}
