/* user_settings.h
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

#ifndef WOLF_USER_SETTINGS_H
#define WOLF_USER_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* STM32F437 demo — wolfSSL user_settings.h                                */
/*                                                                         */
/* Baseline config shared by all build variants (c / hw).                  */
/* Variant-specific overlays come from the Makefile via -D flags.          */
/*                                                                         */
/* Board: STM32439I-EVAL (G-EVAL) with STM32F437IIHx                      */
/* UART:  UART4 on PC10 (TX) / PC11 (RX)  115200 8N1                      */
/* Clock: HSI 16 MHz -> PLL -> 160 MHz SYSCLK                             */
/* ---------------------------------------------------------------------- */

/* Platform / HAL */
#define WOLFSSL_STM32F4
#define WOLFSSL_STM32_CUBEMX
#define STM32_HAL_V2

/* TRNG always on */
#define STM32_RNG
#define STM32_HAL_TIMEOUT 0xFF
#define WC_ASYNC_DEV_SIZE 320+24

/* HW hash/AES — disabled by default; `hw` variant enables via Makefile */
#define NO_STM32_HASH
#define NO_STM32_CRYPTO

/* Single-threaded bare-metal, no filesystem */
#define SINGLE_THREADED
#define NO_FILESYSTEM
#define NO_WRITEV
#define NO_WOLFSSL_SMALL_STACK
#define WOLFSSL_USER_IO
#define NO_MAIN_DRIVER
#define WOLFCRYPT_ONLY

/* Timing resistance */
#define ECC_TIMING_RESISTANT

/* Test settings */
#define BENCH_EMBEDDED
#define NO_MULTIBYTE_PRINT
#define WOLFSSL_IGNORE_FILE_WARN
#define SIZEOF_LONG_LONG 8
#define NO_DEV_RANDOM
#define WOLFSSL_GENSEED_FORTEST

/* AES — GCM and CCM modes */
#define HAVE_AESGCM
#define HAVE_AESCCM
#define GCM_TABLE_4BIT
/* F4 CRYP peripheral does not support 192-bit keys */
#define NO_AES_192

/* Hashes */
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
#define HAVE_HASHDRBG
#define NO_MD4

/* HMAC / KDF */
#define HAVE_HKDF

/* ChaCha20-Poly1305 */
#define HAVE_CHACHA
#define HAVE_POLY1305
#define HAVE_ONE_TIME_AUTH

/* ECC */
#define HAVE_ECC
#define WOLFSSL_SP_MATH
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_SP_384
#define WOLFSSL_SP_521
#define WOLFSSL_SP_SMALL
#define SP_WORD_SIZE 32

/* Curve25519 / Ed25519 */
#define HAVE_CURVE25519
#define HAVE_ED25519

/* Cert buffers for test */
#define USE_CERT_BUFFERS_256
#define WOLFSSL_CERT_GEN
#define WOLFSSL_KEY_GEN

/* Drop unused algorithms */
#define NO_RSA
#define NO_DH
#define NO_DSA
#define NO_RC4
#define NO_PKCS12

/* Thumb2 assembly for ChaCha, Poly1305, Curve25519, SHA-512 */
#define WOLFSSL_ARMASM
#define WOLFSSL_ARMASM_THUMB2
#define WOLFSSL_ARMASM_INLINE
#define WOLFSSL_ARMASM_NO_HW_CRYPTO
#define WOLFSSL_ARMASM_NO_NEON
#define WOLFSSL_NO_HASH_RAW
#define WOLFSSL_NO_VAR_ASSIGN_REG
#define WOLFSSL_ARMASM_AES_BLOCK_INLINE
#define WC_OMIT_FRAME_POINTER

/* Time — no RTC, bypass certificate date checking */
#define NO_ASN_TIME
#define WOLFSSL_USER_CURRTIME

/* Error strings */
#define HAVE_ERRNO
#define WOLFSSL_GMTIME

#ifdef __cplusplus
}
#endif

#endif /* WOLF_USER_SETTINGS_H */
