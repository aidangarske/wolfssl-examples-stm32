/* user_settings.h
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

#ifndef WOLF_USER_SETTINGS_H
#define WOLF_USER_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* STM32U385RG demo — wolfSSL user_settings.h                              */
/*                                                                         */
/* Baseline config shared by all build variants (c / asm / hw / fips).     */
/* Variant-specific overlays come from the Makefile via -D flags.          */
/* ---------------------------------------------------------------------- */

/* Platform / HAL */
#define WOLFSSL_STM32U3
#define WOLFSSL_STM32_CUBEMX
#define STM32_HAL_V2
#define HAL_CONSOLE_UART           /* logs go via semihosting, not UART */

/* TRNG always on — hardware RNG */
#define STM32_RNG

/* HW hash/AES — disabled by default; `hw` and `fips` variants enable via
 * Makefile -D flags. The Makefile cannot -U a #define set here, so we gate
 * these on the variant not already enabling HW (checked via WOLFSSL_STM32_HASH
 * which the hw/fips variants define). */
#if !defined(WOLFSSL_STM32_HASH)
#define NO_STM32_HASH
#endif
#if !defined(WOLFSSL_STM32_AES)
#define NO_STM32_CRYPTO
#endif

/* Single-threaded bare-metal, no filesystem */
#define SINGLE_THREADED
#define NO_FILESYSTEM
#define NO_WRITEV
//#define WOLFSSL_SMALL_STACK
#define WOLFSSL_USER_IO            /* TLS IO via custom callbacks (UART) */
#define WOLFSSL_NO_SOCK            /* skip BSD socket includes */
#define NO_MAIN_DRIVER             /* drivers embedded, we call entry points */
#define NO_THREAD_LS
#define WOLFSSL_IGNORE_FILE_WARN

/* Tight cipher profile: TLS 1.3 only, ECDHE-ECDSA, AES-256-GCM, SHA-384, P-384 */
#define WOLFSSL_TLS13
#define WOLFSSL_NO_TLS12
#define NO_OLD_TLS
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define HAVE_SNI                  /* hostname extension for server name */
#define HAVE_HKDF
#define HAVE_EXTENDED_MASTER
#define HAVE_ENCRYPT_THEN_MAC

/* AES: AES-256-GCM only for the TLS profile. AES-128 stays in test/bench
 * (GMAC test vectors use it) and FIPS (CASTs use it). */
#define HAVE_AESGCM
#define WOLFSSL_AES_256
#define NO_AES_192
#define GCM_TABLE_4BIT             /* balanced size/speed */
#if !defined(WOLFMQTT_DEMO) || defined(HAVE_FIPS)
    #define WOLFSSL_AES_128
#else
    #define NO_AES_CBC             /* TLS 1.3 only uses AES-GCM */
    #define NO_AES_128
#endif

/* Hashes: SHA-384 always (TLS 1.3 transcript for AES-256-GCM-SHA384).
 * WOLFSSL_SHA512 required (SHA-384 reuses the SHA-512 core; FIPS gates
 * the HMAC-SHA512 CAST on WOLFSSL_SHA512). SHA-256 stays on for all
 * builds because asn.c CalcHashId_ex (cert SubjectKeyId hash) is
 * hard-wired to SHA-256 when NO_SHA is set. SHA-224 only for test/FIPS. */
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
#if !defined(WOLFMQTT_DEMO) || defined(HAVE_FIPS)
    #define WOLFSSL_SHA224
#endif
#define NO_MD4
#define NO_MD5
#define NO_SHA                     /* no SHA-1 */

/* ECC: P-384 for the TLS profile. P-256 stays enabled for FIPS because
 * the FIPS ECDHE / ECDSA CASTs use P-256 KAT vectors. With ECC_USER_CURVES,
 * P-256 is on by default unless NO_ECC256 is set. */
#define HAVE_ECC
#define ECC_TIMING_RESISTANT
#define HAVE_ECC384
#define ECC_USER_CURVES
/* With ECC_USER_CURVES, wolfSSL does NOT auto-define NO_ECCnnn for
 * curves we didn't enable. The preferredGroup[] list in tls.c then
 * still includes SECP521R1 (gated only by !defined(NO_ECC521)), and
 * the TLS 1.3 client picks it as the default key_share — which fails
 * with BAD_FUNC_ARG because we don't have SP-521 math compiled. */
#ifndef HAVE_FIPS
    #define NO_ECC256              /* non-FIPS builds: P-384 only */
#endif

/* Math */
#define WOLFSSL_SP_MATH /* only SP math */
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_SP_384
#ifndef HAVE_FIPS
    #define WOLFSSL_SP_NO_256      /* non-FIPS: drop P-256 SP code */
#endif
#define WOLFSSL_SP_SMALL
#define SP_WORD_SIZE 32

/* Drop unused algorithms — biggest size wins */
#define NO_RSA
#define NO_DSA
#define NO_DH
#define NO_RC4
#define NO_DES3
#define NO_HC128
#define NO_RABBIT
#define NO_PSK
#ifndef HAVE_FIPS
#define NO_PWDBASED            /* FIPS requires PBKDF */
#endif
#define NO_PKCS7
#define NO_PKCS8
#define NO_PKCS12
#define NO_SESSION_CACHE
#define NO_WOLFSSL_SERVER          /* client only */

/* Certificate parsing — ECDSA only */
#define WOLFSSL_ASN_TEMPLATE
#define WOLFSSL_CERT_GEN_CACHE

/* Test + benchmark drivers */
#define NO_CRYPT_TEST_LOG_PREFIX
#define BENCH_EMBEDDED             /* for wolfcrypt/benchmark/benchmark.c */
#define WOLFSSL_USER_CURRTIME      /* we provide current_time() in main_bench.c */

/* Bare-metal: wolfSSL timer defaults to time(); we provide a dummy in stubs.c */
#define NO_ASN_TIME                /* skip cert time checks for bring-up test */

/* TEMP: verbose handshake trace */
/* #define DEBUG_WOLFSSL */
#define NO_ERROR_STRINGS

/* ---------------------------------------------------------------------- */
/* wolfMQTT options                                                        */
/* WOLFMQTT_USER_SETTINGS=1 makes wolfMQTT include this very file, so      */
/* both wolfSSL and wolfMQTT options live here (single source of truth).  */
/* ---------------------------------------------------------------------- */
#define ENABLE_MQTT_TLS
#define WOLFMQTT_MAX_PACKET_SIZE 1024
#define MAX_BUFFER_SIZE          1024
/* #define WOLFMQTT_V5 */              /* off for size */
/* #define WOLFMQTT_NONBLOCK */        /* blocking IO */
/* #define WOLFMQTT_DEBUG_SOCKET */    /* uncomment for verbose I/O trace */
/* #define WOLFMQTT_DEBUG_CLIENT */

/* ---------------------------------------------------------------------- */
/* FIPS 140-3 Ready (CONFIG=fips — defines injected by Makefile -D flags)  */
/* ---------------------------------------------------------------------- */
/* When HAVE_FIPS is defined (by Makefile for CONFIG=fips):
 *   HAVE_FIPS_VERSION=7
 *   HAVE_FIPS_VERSION_MAJOR=7
 *
 * The in-core integrity hash is populated after the first run.
 * Build, flash, capture the hash from the FIPS callback output, then
 * set it here and rebuild:
 */
#ifdef HAVE_FIPS
    /* Forward-declare WC_RNG so fips.h can reference it before random.h */
    #ifndef WC_RNG_TYPE_DEFINED
        typedef struct OS_Seed OS_Seed;
        typedef struct WC_RNG WC_RNG;
        #define WC_RNG_TYPE_DEFINED
    #endif

    /* FIPS CAST needs deterministic ECDSA for known-answer tests */
    #define WOLFSSL_ECDSA_SET_K

    /* In-core integrity hash — captured on NUCLEO-U385RG-Q with the
     * FIPS-pinned linker (linker/stm32u385rg_fips.ld). The pinned section
     * order makes this hash stable across rebuilds.
     * If the FIPS source set or compiler flags change, re-capture. */
    #define WOLFCRYPT_FIPS_CORE_HASH_VALUE \
        D3E0D0004D77C4DF849DFDED56B879B1F51AE96547F24FFD28858B68BB08FEFF
#endif

#ifdef __cplusplus
}
#endif

#endif /* WOLF_USER_SETTINGS_H */
