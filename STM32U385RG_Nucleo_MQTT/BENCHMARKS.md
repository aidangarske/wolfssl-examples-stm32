# STM32U385RG_Nucleo_MQTT — Benchmarks

Hardware: NUCLEO-U385RG-Q, STM32U385RG @ 96 MHz (MSIS RC0 / DIV1).
Toolchain: arm-none-eabi-gcc 13.2, `-Os -ffunction-sections -fdata-sections
-fomit-frame-pointer -mfloat-abi=soft`, `-Wl,--gc-sections --specs=nano.specs
--specs=rdimon.specs`. wolfSSL `stm32u3` branch (adds `WOLFSSL_STM32U3`
family macro). Math: `WOLFSSL_SP_MATH` (small SP only). `NO_ERROR_STRINGS`
on. All sizes in bytes; `flash = text + data`.

## Code size — TLS 1.3 mTLS MQTT app (no test/bench code)

The customer-facing number: full TLS 1.3 mutual-auth MQTT client with
AES-128-GCM-SHA256 over secp384r1, no other algorithms, no test or
benchmark drivers linked in.

| Variant | text    | data | bss    | flash   | Δ vs `c` |
|---------|--------:|-----:|-------:|--------:|---------:|
| c    | 129,920 | 148  | 25,340 | **130,068** |  ref     |
| asm  | 133,236 | 148  | 25,340 | **133,384** | +3,316   |
| hw   | 124,924 | 148  | 25,508 | **125,072** | **-4,996 (-3.8%)** |
| fips | 175,392 | 152  | 25,464 | **175,544** | +45,476  |

App variants (non-FIPS) are trimmed for TLS 1.3 client only with
AES-256-GCM-SHA384 and ECDSA-ECDHE P-384: no AES-128, no AES-CBC, no
SHA-224. Gates fire on `WOLFMQTT_DEMO && !HAVE_FIPS`.

The FIPS variant uses the same `--specs=nano.specs --specs=rdimon.specs`
profile as the others but keeps AES-128, AES-CBC, and SHA-224 because
the FIPS CAST KATs need them. The +57 KB delta vs trimmed `c` is the
FIPS module: `fips.c` + `fips_test.c` (POSTs/CASTs), the integrity
boundary markers (`wolfcrypt_first.o`, `wolfcrypt_last.o`), and the
dependency expansion required by FIPS CASTs (P-256 alongside P-384,
SHA-512 for HMAC-SHA512 KAT, SHA-224 for `WC_MIN_DIGEST_SIZE`).

## Per-component breakdown (`app-hw`, 125,072 bytes flash)

Source-attribution from `arm-none-eabi-nm --print-size --size-sort -l`,
filtered to `T/t/R/r/D/d` (text + rodata + data; flash-resident only).
% is of total flash (125,072).

| Component                         | Bytes  |   %   |
|-----------------------------------|-------:|------:|
| **wolfCrypt** (`wolfcrypt/src/`)  | 52,506 | 42.0% |
| **wolfSSL TLS** (`src/`)          | 34,703 | 27.7% |
| **STM32 HAL** (HAL_HASH, HAL_CRYP, HAL_UART, HAL_RCC, ...) | 12,949 | 10.4% |
| **newlib** (libc, nano)           | 12,779 | 10.2% |
| **wolfMQTT** (`mqtt_client.c`, `mqtt_packet.c`, `mqtt_socket.c`) | 5,572 |  4.5% |
| Demo glue (`main.c`, `hw_init.c`, `stubs.c`, `semihost.c`, `it.c`, MSP) | 1,422 | 1.1% |
| Embedded certs (CA + client cert + key, P-384 DER) | 1,239 | 1.0% |
| libgcc / libc-arch (memcpy/memmove/strcmp ASM) | 766 | 0.6% |
| UART transport (`uart_net.c` code) | 536 | 0.4% |
| CMSIS (startup + system_stm32u3xx.c) | 152 | 0.1% |
| **Total attributed**              | **122,624** | **98.0%** |

Remainder (~2.4 KB) is small symbols, vector table, interrupt stubs,
and section padding.

Application crypto (wolfCrypt + wolfSSL TLS + wolfMQTT) is **92.8 KB
(74% of flash)**. Standard-library glue (newlib + libgcc + arch) is
**13.5 KB (11%)**.

(wolfBoot is a separate 35 KB binary; `wolfboot.bin` is flashed at
0x08000000 alongside the signed app at 0x08010000. See README §
"wolfBoot Verified Boot" for the layout.)

### Top consumers within wolfCrypt

| File          | Bytes  |
|---------------|-------:|
| asn.c         | 15,226 |
| sp_cortexm.c  | 11,830 |
| sha512.c      | 10,214 |
| ecc.c         | 10,051 |
| sp_int.c      |  6,386 |
| random.c      |  3,162 |
| aes.c         |  3,034 |
| hmac.c        |  2,264 |
| port/st/stm32.c | 1,228 |

Notes:
- `sha512.c` stays in software because the U3 HAL HASH peripheral only
  accelerates SHA-1/SHA-2-256. SHA-384 / SHA-512 use the C path.
- `asn.c` is mostly cert-chain parsing — required for mutual auth.
- `sp_cortexm.c` + `ecc.c` is the P-384 ECC path (KAS + ECDSA).

### Notable optimization: `--specs=nano.specs`

Switched from `--specs=rdimon.specs` alone to
`--specs=nano.specs --specs=rdimon.specs`. This pulls **newlib-nano**
which omits `%f` / `%e` / `%g` printf support, double-precision
soft-float math, and the multi-precision `mprec.c`:

| Subsystem         | Before  | After   | Δ        |
|-------------------|--------:|--------:|---------:|
| newlib (libc)     | 34,589  | 12,355  | **-22,234** |
| libgcc            |  6,934  |    766  | **-6,168**  |
| **app-hw flash**  | **168,012** | **141,396** | **-26,616 (-15.8%)** |

That `-15.8%` is the single biggest size win in the demo and is
applied to all non-FIPS variants.

## Code size — wolfCrypt test (`make test`)

| Variant | text    | data | bss    | flash   |
|---------|--------:|-----:|-------:|--------:|
| c    | 116,696 | 116  | 21,084 | 116,812 |
| asm  | 120,012 | 116  | 21,084 | 120,128 |
| hw   | 107,636 | 116  | 21,244 | 107,752 |
| fips | 159,692 | 120  | 21,208 | 159,812 |

## Code size — wolfCrypt bench (`make bench`)

| Variant | text   | data | bss    | flash  |
|---------|-------:|-----:|-------:|-------:|
| c    |  79,824 | 352  | 21,144 |  80,176 |
| asm  |  83,152 | 352  | 21,144 |  83,504 |
| hw   |  70,720 | 352  | 21,304 |  71,072 |
| fips | 122,764 | 356  | 21,260 | 123,120 |

## Runtime — wolfCrypt benchmark (`asm` vs `hw`)

`asm` = pure software (Cortex-M Thumb2 ASM SP math; AES & SHA in C).
`hw`  = STM32 `HAL_HASH` for SHA-2 + STM32 `HAL_CRYP` for AES.

### AES-CBC / AES-GCM (1 KiB blocks)

| Algorithm        |     asm    |    hw     |  speedup |
|------------------|-----------:|----------:|---------:|
| AES-128-GCM enc  |   607 KiB/s | 4.83 MiB/s | **8.2×** |
| AES-128-GCM dec  |   391 KiB/s | 4.79 MiB/s | **12.5×** |
| AES-256-GCM enc  |   450 KiB/s | 4.49 MiB/s | **10.2×** |
| AES-256-GCM dec  |   319 KiB/s | 4.45 MiB/s | **14.3×** |
| AES-128-CBC enc  | 1,001 KiB/s | 5.14 MiB/s | **5.3×**  |
| AES-128-CBC dec  |   758 KiB/s | 5.12 MiB/s | **6.9×**  |
| AES-256-CBC enc  |   649 KiB/s | 4.77 MiB/s | **7.5×**  |
| AES-256-CBC dec  |   498 KiB/s | 4.74 MiB/s | **9.7×**  |
| GMAC (4-bit table) | 1.49 MiB/s | 10.59 MiB/s | **7.1×**  |

### Hashes

| Algorithm | asm        | hw          | speedup |
|-----------|-----------:|------------:|--------:|
| SHA-256   | 1.63 MiB/s | 17.96 MiB/s | **11.0×** |
| SHA-384   |  773 KiB/s |   774 KiB/s | ~same (HW does SHA-2-256 only on U3) |
| RNG (DRBG)|  653 KiB/s | 1.29 MiB/s | 2.0× |

### ECC P-384 (Cortex-M ASM SP math, identical for asm and hw)

| Operation       | ops/sec |
|-----------------|--------:|
| key gen         |  12.5   |
| ECDHE agree     |   5.85  |
| ECDSA sign      |  10.05  |
| ECDSA verify    |   4.49  |

ECC stays in software for both variants — STM32U3 has a PKA peripheral
that's not yet wired into the wolfSSL port (follow-up). Cortex-M ASM
SP math is ~5× over pure C P-384 (12.5 vs 2.5 ops/sec key-gen).

## Summary

For a TLS 1.3 mTLS MQTT client on STM32U385RG @ 96 MHz:

- **Total binary 141 KB flash** (`app-hw`); 14.6 KB of that is
  newlib + libgcc + libgloss / semihosting glue. Application crypto
  (wolfCrypt + wolfSSL TLS + wolfMQTT) is **107.5 KB** (76% of flash).
- HW HASH + HW AES on U3 saves **5.4 KB** vs ASM-only (and is **10×**
  faster on AES-GCM, **11×** on SHA-256).
- nano.specs is the biggest single size win (**-26.6 KB**).
- ECC P-384 software path is the runtime bottleneck (single-digit
  ops/sec); PKA peripheral integration is the next big win.

Re-generate with `make size-report` after building, or
`make bench CONFIG=<asm|hw>` for runtime numbers.
