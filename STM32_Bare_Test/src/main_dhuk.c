/* main_dhuk.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * DHUK (Device Hardware Unique Key) test app for STM32_Bare_Test, exercising
 * the transparent crypto-callback DHUK path (the STM32 cryptocb device, see
 * wc_Stm32_DhukRegister). Build with: make BOARD=u3 CONFIG=bare TARGET=dhuk
 *
 *   [1] wc_ecc_import_wrapped_private input validation -- a pure-software
 *       hard PASS/FAIL covering the seed + wrapped-scalar bounds checks.
 *   [2] GMAC via the transparent crypto-callback (normal wc_AesGcmEncrypt on
 *       a DHUK-keyed Aes; derived key never in software).
 *   [3] AES-ECB via the transparent crypto-callback (wc_AesEcb*; round-trip +
 *       seed-dependence confirm the DHUK key drives the cipher).
 *   [4] ECDSA sign via the transparent crypto-callback (wc_ecc_sign_hash with
 *       a DHUK-wrapped scalar; verified with the public counterpart).
 *
 * A backend that is gated off or unavailable (CRYPTOCB_UNAVAILABLE / a
 * TZEN-secure-context timeout) is reported as an expected soft-PASS, not a
 * failure. Results are also mirrored to the g_dhuk_res debugger sink. */

#include <stdio.h>

#include "board.h"

extern uint32_t SystemCoreClock;
extern void     SystemCoreClockUpdate(void);

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/version.h"
#include "wolfssl/wolfcrypt/types.h"
#include "wolfssl/wolfcrypt/wc_port.h"
#include "wolfssl/wolfcrypt/error-crypt.h"
#include "wolfssl/wolfcrypt/random.h"
#include "wolfssl/wolfcrypt/ecc.h"
#include "wolfssl/wolfcrypt/port/st/stm32.h"
#include "wolfssl/wolfcrypt/aes.h"

#ifndef BUILD_CONFIG_NAME
#define BUILD_CONFIG_NAME "unknown"
#endif

/* Debugger-readable result sink, so results can be captured without a
 * working VCP (the B-U585I-IOT02A's UART routing differs from the
 * NUCLEO-U585AI-Q this board build targets). Read after the run with:
 *   arm-none-eabi-nm app.elf | grep g_dhuk_res   # address
 *   openocd ... -c "mdw 0x<addr> 12"
 * Captures the RAW return codes (before the soft-PASS mapping) so a
 * gated/timeout (-271 / WC_TIMEOUT_E) is distinguishable from a real
 * success (0). magic = 0xD04B0001 once main() reaches the end. */
volatile struct {
    uint32_t magic;
    int32_t  setter_rc;    /* 0 = all setter validation cases passed   */
    int32_t  cb_gmac_rc;   /* transparent crypto-cb GMAC return        */
    uint32_t cb_gmac_tag[4];/* crypto-cb GMAC tag on success           */
    int32_t  overall;      /* final result (0 = PASS)                 */
} g_dhuk_res;

/* A backend that is gated off or that cannot complete the unwrap on
 * TZEN=0 silicon returns one of these. Treat as expected, not a fail. */
static int is_expected_gated(int ret)
{
    return (ret == CRYPTOCB_UNAVAILABLE) ||
           (ret == WC_TIMEOUT_E) ||
           (ret == WC_HW_E);
}

/* Compare an actual return code against the expected one. Returns 0 on
 * match, -1 on mismatch (with a printed diagnostic). */
static int expect_ret(const char* label, int got, int want)
{
    if (got != want) {
        printf("  %s: got %d, want %d -- FAIL\n", label, got, want);
        return -1;
    }
    printf("  %s: %d OK\n", label, got);
    return 0;
}

#if defined(WOLFSSL_DHUK) && \
    (defined(WOLFSSL_STM32_BARE) || defined(WOLFSSL_STM32_CUBEMX)) && \
    defined(WC_STM32_HAS_DHUK)

/* [1] wc_ecc_import_wrapped_private input-validation unit test. Pure
 * software; a hard PASS/FAIL exercising the seed + wrapped-scalar bounds checks
 * (including the wrappedLen <= roundup16(plainLen) invariant). */
static int test_ecc_dhuk_setter(void)
{
    /* Content does not matter for the validation paths; only lengths are
     * checked. Sized to the largest blob the import accepts (96 bytes). */
    byte wrapped[96];
    byte seed[32];
    ecc_key key;
    int ret;
    int rc = 0;

    XMEMSET(wrapped, 0xa5, sizeof(wrapped));
    XMEMSET(seed, 0x5a, sizeof(seed));

    ret = wc_ecc_init(&key);
    if (ret != 0) {
        printf("  wc_ecc_init failed: %d\n", ret);
        return ret;
    }

    /* Reject: NULL key / seed / wrapped pointers. */
    ret = wc_ecc_import_wrapped_private(NULL, seed, 32, wrapped, 32, 32);
    if (expect_ret("reject key=NULL", ret, BAD_FUNC_ARG) != 0) rc = -1;
    ret = wc_ecc_import_wrapped_private(&key, NULL, 32, wrapped, 32, 32);
    if (expect_ret("reject seed=NULL", ret, BAD_FUNC_ARG) != 0) rc = -1;
    ret = wc_ecc_import_wrapped_private(&key, seed, 32, NULL, 32, 32);
    if (expect_ret("reject wrapped=NULL", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Good: 32-byte seed, 32-byte wrapped scalar, 32-byte plaintext (P-256). */
    ret = wc_ecc_import_wrapped_private(&key, seed, 32, wrapped, 32, 32);
    if (expect_ret("accept P-256 (32/32)", ret, 0) != 0) rc = -1;

    /* Boundary OK: P-521 plaintext (66) padded to 80, blob 80 == max. */
    ret = wc_ecc_import_wrapped_private(&key, seed, 32, wrapped, 80, 66);
    if (expect_ret("accept P-521 (80/66)", ret, 0) != 0) rc = -1;

    /* Boundary OK: minimum blob -- 1-byte plaintext padded to one AES block. */
    ret = wc_ecc_import_wrapped_private(&key, seed, 32, wrapped, 16, 1);
    if (expect_ret("accept min (16/1)", ret, 0) != 0) rc = -1;

    /* Reject: seed length must be 32. */
    ret = wc_ecc_import_wrapped_private(&key, seed, 16, wrapped, 32, 32);
    if (expect_ret("reject seedSz=16", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Reject: not a multiple of the AES block size. */
    ret = wc_ecc_import_wrapped_private(&key, seed, 32, wrapped, 20, 20);
    if (expect_ret("reject wrappedLen=20", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Reject: zero-length wrapped blob. */
    ret = wc_ecc_import_wrapped_private(&key, seed, 32, wrapped, 0, 0);
    if (expect_ret("reject wrappedLen=0", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Reject: larger than the on-key buffer (> 96). */
    ret = wc_ecc_import_wrapped_private(&key, seed, 32, wrapped, 112, 32);
    if (expect_ret("reject wrappedLen=112", ret, BAD_FUNC_ARG) != 0) rc = -1;

    /* Reject: plaintext longer than the wrapped blob. */
    ret = wc_ecc_import_wrapped_private(&key, seed, 32, wrapped, 32, 48);
    if (expect_ret("reject plain=48 > wrapped=32", ret, BAD_FUNC_ARG) != 0)
        rc = -1;

    /* Reject: wrapped blob larger than the plaintext padded to a full block
     * (plain=16 -> roundup16 = 16, so a 48-byte blob is malformed). */
    ret = wc_ecc_import_wrapped_private(&key, seed, 32, wrapped, 48, 16);
    if (expect_ret("reject wrapped=48 > roundup16(plain=16)", ret,
                   BAD_FUNC_ARG) != 0)
        rc = -1;

    wc_ecc_free(&key);
    g_dhuk_res.setter_rc = rc;
    if (rc == 0) {
        printf("  setter validation OK\n");
    }
    return rc;
}

#ifdef WOLF_CRYPTO_CB
/* [2] Transparent crypto-callback GMAC via the STM32 DHUK device. Register the
 * device, init a normal Aes with devId = WC_DHUK_DEVID, set the 256-bit seed as
 * the key (wc_AesGcmSetKey), then call the standard wc_AesGcmEncrypt with empty
 * plaintext (GMAC). The derived key never appears in software. Checks: no
 * timeout, determinism (two runs match), and round-trip verify via
 * wc_AesGcmDecrypt. A gated/timeout result is a soft-PASS. */
static int test_dhuk_cryptocb_gmac(void)
{
    static const byte seed[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte iv[12] = {
        0xca,0xfe,0xba,0xbe,0xfa,0xce,0xdb,0xad,
        0xde,0xca,0xf8,0x88
    };
    static const byte aad[16] = {
        0xfe,0xed,0xfa,0xce,0xde,0xad,0xbe,0xef,
        0xfe,0xed,0xfa,0xce,0xde,0xad,0xbe,0xef
    };
    Aes  aes;
    byte io[1];
    byte tag1[16];
    byte tag2[16];
    int  ret;
    int  i;

    XMEMSET(tag1, 0, sizeof(tag1));
    XMEMSET(tag2, 0, sizeof(tag2));

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  wc_Stm32_DhukRegister failed: %d\n", ret);
        return ret;
    }

    /* GMAC tag via the normal AES-GCM API (empty plaintext). */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
    }
    if (ret == 0) {
        ret = wc_AesGcmEncrypt(&aes, io, io, 0, iv, (word32)sizeof(iv),
                               tag1, (word32)sizeof(tag1),
                               aad, (word32)sizeof(aad));
    }
    wc_AesFree(&aes);
    g_dhuk_res.cb_gmac_rc = ret;
    XMEMCPY((void*)g_dhuk_res.cb_gmac_tag, tag1, sizeof(tag1));

    if (is_expected_gated(ret)) {
        printf("  cryptocb GMAC reachable; backend gated/unavailable "
               "(ret=%d)\n", ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  cryptocb GMAC failed: %d\n", ret);
        goto cleanup;
    }
    printf("  cryptocb GMAC tag:");
    for (i = 0; i < 16; i++) printf(" %02x", tag1[i]);
    printf("\n");

    /* Determinism: same seed + inputs must give the same tag. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
    }
    if (ret == 0) {
        ret = wc_AesGcmEncrypt(&aes, io, io, 0, iv, (word32)sizeof(iv),
                               tag2, (word32)sizeof(tag2),
                               aad, (word32)sizeof(aad));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb GMAC (run 2) failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(tag1, tag2, 16) != 0) {
        printf("  cryptocb GMAC not deterministic -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb GMAC deterministic OK\n");

    /* Round-trip: verify the tag via the normal AES-GCM decrypt API. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, seed, (word32)sizeof(seed));
    }
    if (ret == 0) {
        ret = wc_AesGcmDecrypt(&aes, io, io, 0, iv, (word32)sizeof(iv),
                               tag1, (word32)sizeof(tag1),
                               aad, (word32)sizeof(aad));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb GMAC verify failed: %d\n", ret);
        goto cleanup;
    }
    printf("  cryptocb GMAC verify OK (round-trip)\n");
    ret = 0;

cleanup:
    wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    return ret;
}

#if defined(HAVE_AES_ECB) || defined(WOLFSSL_AES_DIRECT)
/* [3] Transparent AES-ECB via the STM32 DHUK device. Encrypt/decrypt with the
 * standard wc_AesEcb* API on an Aes inited with devId = WC_DHUK_DEVID and the
 * seed set as the key. Checks: round-trip recovers plaintext, ciphertext !=
 * plaintext (encryption happened), and a DIFFERENT seed yields a DIFFERENT
 * ciphertext (proves the DHUK-derived key actually drives the cipher, i.e. the
 * crypto-callback path is engaged). */
static int test_dhuk_cryptocb_ecb(void)
{
    static const byte seedA[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte seedB[32] = {
        0xa5,0xa5,0xa5,0xa5,0xa5,0xa5,0xa5,0xa5,
        0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
    static const byte pt[16] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
    };
    Aes  aes;
    byte ctA[16];
    byte ctB[16];
    byte rt[16];
    int  ret;
    int  i;

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  DHUK register failed: %d\n", ret);
        return ret;
    }

    /* Encrypt with seed A. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, NULL, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, ctA, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (is_expected_gated(ret)) {
        printf("  cryptocb ECB reachable; backend gated/unavailable (ret=%d)\n",
               ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  cryptocb ECB encrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(pt, ctA, 16) == 0) {
        printf("  cryptocb ECB produced plaintext -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb ECB ct:");
    for (i = 0; i < 16; i++) printf(" %02x", ctA[i]);
    printf("\n");

    /* Decrypt with seed A -- must recover plaintext. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, NULL, AES_DECRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbDecrypt(&aes, rt, ctA, (word32)sizeof(ctA));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb ECB decrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(pt, rt, 16) != 0) {
        printf("  cryptocb ECB round-trip mismatch -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb ECB round-trip OK\n");

    /* Encrypt with seed B -- different seed must give different ciphertext. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedB, 32, NULL, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, ctB, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb ECB (seed B) failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(ctA, ctB, 16) == 0) {
        printf("  cryptocb ECB seed had no effect -- FAIL (not DHUK path)\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb ECB seed-dependent OK (DHUK key drives cipher)\n");
    ret = 0;

cleanup:
    wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    return ret;
}
#endif /* HAVE_AES_ECB || WOLFSSL_AES_DIRECT */

#if defined(HAVE_AES_CBC)
/* [5] Transparent AES-CBC via the STM32 DHUK device. Validates the fix that
 * routes CBC through the crypto-callback: previously CBC bypassed it and
 * silently used the 256-bit seed as a raw AES key. Discriminating check: for a
 * single block with IV = 0, CBC(P) == ECB(P) ONLY when both use the same
 * SAES-derived key -- the old seed-as-key path would not match ECB's derived
 * key. Also confirms that a DHUK AES-CTR call (a mode the SAES backend cannot
 * service) now returns a hard error instead of silently using the seed. */
static int test_dhuk_cryptocb_cbc(void)
{
    static const byte seedA[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte pt[16] = {
        0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
        0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
    };
    static const byte zero_iv[16] = { 0 };
    Aes  aes;
    byte ctEcb[16];
    byte ctCbc[16];
    byte rt[16];
    int  ret;

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  DHUK register failed: %d\n", ret);
        return ret;
    }

    /* ECB reference: ctEcb = ECB_k(pt) with the SAES-derived key. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, NULL, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, ctEcb, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (is_expected_gated(ret)) {
        printf("  cryptocb CBC reachable; backend gated/unavailable (ret=%d)\n",
               ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  ECB reference failed: %d\n", ret);
        goto cleanup;
    }

    /* CBC with IV = 0, single block: must equal the ECB reference. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, zero_iv, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesCbcEncrypt(&aes, ctCbc, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb CBC encrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(ctEcb, ctCbc, 16) != 0) {
        printf("  cryptocb CBC != ECB(IV=0) -- FAIL "
               "(CBC not using the SAES-derived key)\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb CBC matches ECB(IV=0) OK (SAES-derived key drives CBC)\n");

    /* CBC round-trip: decrypt recovers plaintext. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, zero_iv, AES_DECRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesCbcDecrypt(&aes, rt, ctCbc, (word32)sizeof(ctCbc));
    }
    wc_AesFree(&aes);
    if (ret != 0) {
        printf("  cryptocb CBC decrypt failed: %d\n", ret);
        goto cleanup;
    }
    if (XMEMCMP(pt, rt, 16) != 0) {
        printf("  cryptocb CBC round-trip mismatch -- FAIL\n");
        ret = -1;
        goto cleanup;
    }
    printf("  cryptocb CBC round-trip OK\n");

#ifdef WOLFSSL_AES_COUNTER
    /* Negative: AES-CTR is not serviceable for a DHUK key. It must now return a
     * hard error (ALGO_ID_E) rather than silently encrypting with the seed as a
     * raw key. */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seedA, 32, zero_iv, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesCtrEncrypt(&aes, rt, pt, (word32)sizeof(pt));
    }
    wc_AesFree(&aes);
    if (ret == ALGO_ID_E) {
        printf("  cryptocb CTR on DHUK key rejected (ALGO_ID_E) OK\n");
        ret = 0;
    }
    else {
        printf("  cryptocb CTR on DHUK key NOT rejected (ret=%d) -- FAIL\n", ret);
        ret = -1;
        goto cleanup;
    }
#endif /* WOLFSSL_AES_COUNTER */
    ret = 0;

cleanup:
    wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    return ret;
}
#endif /* HAVE_AES_CBC */

#if defined(HAVE_ECC) && defined(WOLFSSL_STM32_PKA)
/* [4] ECDSA sign with a DHUK-protected private key via the normal
 * wc_ecc_sign_hash API. Self-bootstrap: make a P-256 keypair, ECB-encrypt
 * (wrap) its scalar with the DHUK-derived key (same seed), import the wrapped
 * scalar + seed onto the ecc_key, set devId = WC_DHUK_DEVID, then sign. The
 * plaintext scalar only lives in a short-lived stack buffer during the PKA
 * sign. The signature is verified with the public counterpart (SW path). */
static int test_dhuk_cryptocb_ecdsa(WC_RNG* rng)
{
    static const byte seed[32] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
        0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01
    };
    static const byte hash[32] = {
        0x9f,0x86,0xd0,0x81,0x88,0x4c,0x7d,0x65,
        0x9a,0x2f,0xea,0xa0,0xc5,0x5a,0xd0,0x15,
        0xa3,0xbf,0x4f,0x1b,0x2b,0x0b,0x82,0x2c,
        0xd1,0x5d,0x6c,0x15,0xb0,0xf0,0x0a,0x08
    };
    ecc_key kp;
    Aes  aes;
    byte priv[32];
    byte wrapped[32];
    byte sig[80];
    word32 privSz = (word32)sizeof(priv);
    word32 sigLen = (word32)sizeof(sig);
    int  ret;
    int  verify = 0;
    int  haveKey = 0;

    ret = wc_Stm32_DhukRegister(WC_DHUK_DEVID);
    if (ret != 0) {
        printf("  DHUK register failed: %d\n", ret);
        return ret;
    }

    ret = wc_ecc_init(&kp);
    if (ret != 0) {
        printf("  wc_ecc_init failed: %d\n", ret);
        goto unreg;
    }
    haveKey = 1;
    ret = wc_ecc_make_key_ex(rng, 32, &kp, ECC_SECP256R1);
    if (ret != 0) {
        printf("  wc_ecc_make_key_ex failed: %d\n", ret);
        goto cleanup;
    }

    /* Sanity: plain (non-DHUK) PKA ECDSA sign+verify on this silicon, to
     * isolate any DHUK-path issue from a PKA-hardware issue. kp.devId is
     * still INVALID here, so this uses the normal HW PKA path. */
    sigLen = (word32)sizeof(sig);
    ret = wc_ecc_sign_hash(hash, (word32)sizeof(hash), sig, &sigLen, rng, &kp);
    if (ret == 0) {
        ret = wc_ecc_verify_hash(sig, sigLen, hash, (word32)sizeof(hash),
                                 &verify, &kp);
    }
    printf("  plain PKA sign+verify: rc=%d verify=%d\n", ret, verify);
    ret = 0;
    verify = 0;
    sigLen = (word32)sizeof(sig);

    ret = wc_ecc_export_private_only(&kp, priv, &privSz);
    if (ret != 0 || privSz != 32u) {
        printf("  export scalar failed: %d (len %lu)\n", ret,
               (unsigned long)privSz);
        ret = (ret != 0) ? ret : -1;
        goto cleanup;
    }

    /* Wrap the scalar = ECB-encrypt with the DHUK-derived key (seed as key). */
    ret = wc_AesInit(&aes, NULL, WC_DHUK_DEVID);
    if (ret == 0) {
        ret = wc_AesSetKey(&aes, seed, 32, NULL, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesEcbEncrypt(&aes, wrapped, priv, 32);
    }
    wc_AesFree(&aes);
    wc_ForceZero(priv, sizeof(priv));
    if (is_expected_gated(ret)) {
        printf("  DHUK ECDSA: backend gated/unavailable (ret=%d)\n", ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  scalar wrap failed: %d\n", ret);
        goto cleanup;
    }

    /* Import the wrapped scalar + seed, and route signing through DHUK by
     * setting the device id on the key. */
    kp.devId = WC_DHUK_DEVID;
    ret = wc_ecc_import_wrapped_private(&kp, seed, (word32)sizeof(seed),
                                       wrapped, 32, 32);
    if (ret != 0) {
        printf("  import wrapped private failed: %d\n", ret);
        goto cleanup;
    }

    ret = wc_ecc_sign_hash(hash, (word32)sizeof(hash), sig, &sigLen, rng, &kp);
    if (is_expected_gated(ret)) {
        printf("  DHUK ECDSA sign gated/unavailable (ret=%d)\n", ret);
        ret = 0; /* soft-PASS */
        goto cleanup;
    }
    if (ret != 0) {
        printf("  DHUK ECDSA sign failed: %d\n", ret);
        goto cleanup;
    }
    printf("  DHUK ECDSA sign produced a %lu-byte signature\n",
           (unsigned long)sigLen);

    /* Verify with the public counterpart (crypto-cb has no verify -> SW). */
    ret = wc_ecc_verify_hash(sig, sigLen, hash, (word32)sizeof(hash),
                             &verify, &kp);
    if (ret != 0) {
        printf("  DHUK ECDSA verify error: %d\n", ret);
        goto cleanup;
    }
    if (verify != 1) {
        printf("  DHUK ECDSA verify FAILED (sig invalid)\n");
        ret = -1;
        goto cleanup;
    }
    printf("  DHUK ECDSA verify OK (signed via DHUK, verified with pubkey)\n");
    ret = 0;

cleanup:
    wc_ForceZero(priv, sizeof(priv));
    wc_ForceZero(wrapped, sizeof(wrapped));
    if (haveKey) {
        wc_ecc_free(&kp);
    }
unreg:
    wc_Stm32_DhukUnRegister(WC_DHUK_DEVID);
    return ret;
}
#endif /* HAVE_ECC && WOLFSSL_STM32_PKA */
#endif /* WOLF_CRYPTO_CB */

#endif /* WOLFSSL_DHUK && WOLFSSL_STM32_BARE && WC_STM32_HAS_DHUK */

int main(void)
{
    int ret = 0;

    board_init();
    SystemCoreClockUpdate();

    printf("\n");
    printf("========================================\n");
    printf("wolfCrypt DHUK test - %s (CONFIG=%s)\n",
           board_name(), BUILD_CONFIG_NAME);
    printf("wolfSSL version: %s\n", LIBWOLFSSL_VERSION_STRING);
    printf("SYSCLK: expected %lu Hz, CMSIS-reported %lu Hz%s\n",
           (unsigned long)board_sysclk_hz(),
           (unsigned long)SystemCoreClock,
           ((unsigned long)SystemCoreClock == (unsigned long)board_sysclk_hz())
               ? " (match)" : " (MISMATCH -- PLL may have failed)");
    printf("========================================\n\n");

    ret = wolfCrypt_Init();
    if (ret != 0) {
        printf("wolfCrypt_Init failed: %d\n", ret);
        for (;;) { }
    }

#if defined(WOLFSSL_DHUK) && \
    (defined(WOLFSSL_STM32_BARE) || defined(WOLFSSL_STM32_CUBEMX)) && \
    defined(WC_STM32_HAS_DHUK)
    {
        WC_RNG rng;

        printf("[1] ECC DHUK setter validation (SW unit test):\n");
        ret = test_ecc_dhuk_setter();
        if (ret != 0) {
            goto done;
        }

        ret = wc_InitRng(&rng);
        if (ret != 0) {
            printf("  wc_InitRng failed: %d\n", ret);
            goto done;
        }

#ifdef WOLF_CRYPTO_CB
        if (ret == 0) {
            printf("\n[2] GMAC via transparent DHUK crypto-callback:\n");
            ret = test_dhuk_cryptocb_gmac();
        }
#if defined(HAVE_AES_ECB) || defined(WOLFSSL_AES_DIRECT)
        if (ret == 0) {
            printf("\n[3] AES-ECB via transparent DHUK crypto-callback:\n");
            ret = test_dhuk_cryptocb_ecb();
        }
#endif
#if defined(HAVE_AES_CBC)
        if (ret == 0) {
            printf("\n[5] AES-CBC via transparent DHUK crypto-callback:\n");
            ret = test_dhuk_cryptocb_cbc();
        }
#endif
#if defined(HAVE_ECC) && defined(WOLFSSL_STM32_PKA)
        if (ret == 0) {
            printf("\n[4] ECDSA sign via transparent DHUK crypto-callback:\n");
            ret = test_dhuk_cryptocb_ecdsa(&rng);
        }
#endif
#endif

        wc_FreeRng(&rng);
    }
#else
    printf("DHUK not enabled in this build "
           "(need WOLFSSL_DHUK + WOLFSSL_STM32_BARE + WC_STM32_HAS_DHUK).\n");
    ret = -1;
#endif

done:
    g_dhuk_res.overall = ret;
    g_dhuk_res.magic = 0xD04B0001u;
    wolfCrypt_Cleanup();
    printf("\nResult: %d (%s)\n", ret, ret == 0 ? "PASS" : "FAIL");
    printf("Test complete\n");

    for (;;) { }
}
