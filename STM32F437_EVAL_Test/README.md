# STM32F437 wolfCrypt Test & Benchmark

Bare-metal wolfCrypt self-test and benchmark for the **STM32439I-EVAL (G-EVAL)**
board with STM32F437IIHx. Demonstrates wolfCrypt with STM32 hardware crypto
(CRYP, HASH, RNG) and Thumb2 assembly optimizations.

## Board

| Item | Value |
|------|-------|
| MCU | STM32F437IIHx (Cortex-M4, 160 MHz) |
| Flash | 2 MB |
| SRAM | 192 KB + 64 KB CCMRAM |
| UART | UART4 — PC10 (TX), PC11 (RX), 115200 8N1 |
| Clock | HSI 16 MHz → PLL → 160 MHz |
| Crypto HW | CRYP (AES), HASH (SHA-1/256), RNG |

## Quick Start

```bash
# Build wolfCrypt test (pure C math)
make CONFIG=c test

# Build wolfCrypt test (STM32 HW HASH + CRYP)
make CONFIG=hw test

# Flash via OpenOCD (single STLINK)
make flash

# Flash with specific STLINK serial (multi-probe)
make flash STLINK_SERIAL=50FF70064977535534290587

# Build and run benchmark
make CONFIG=hw bench
make flash
```

## Build Variants

| CONFIG | SP Math | AES | HASH | Description |
|--------|---------|-----|------|-------------|
| `c` | sp_c32.c (C) | Software | Software | Pure C, no HW acceleration |
| `hw` | sp_c32.c (C) | STM32 CRYP | STM32 HASH | Hardware crypto offload |

## Algorithms Enabled

- **Symmetric**: AES-128/256 (GCM, CCM, CBC), ChaCha20-Poly1305, 3DES
- **Hash**: SHA-1, SHA-256, SHA-384, SHA-512, MD5, HMAC
- **ECC**: P-256, P-384, P-521 (SP math)
- **Key Exchange**: Curve25519, ECDHE
- **Signatures**: Ed25519, ECDSA
- **Assembly**: Thumb2 inline ASM for ChaCha, Poly1305, Curve25519, SHA-512

RSA and DH are disabled to fit in 192 KB SRAM.

## Directory Layout

```
STM32F437_EVAL_Test/
├── Makefile                 # Multi-config build system
├── user_settings.h          # wolfSSL configuration (baseline)
├── README.md
├── .gitignore
├── src/
│   ├── main_test.c          # wolfCrypt self-test entry
│   ├── main_bench.c         # wolfCrypt benchmark entry
│   ├── hw_init.c            # Clock, UART, GPIO, crypto MSP init
│   ├── stubs.c              # _sbrk, time() stubs
│   ├── stm32f4xx_hal_conf.h # Minimal HAL module selection
│   ├── stm32f4xx_it.c       # Interrupt handlers (SysTick)
│   ├── system_stm32f4xx.c   # CMSIS SystemInit
│   └── startup_stm32f437xx.s # Vector table + reset handler
└── linker/
    └── stm32f437_flat.ld    # 2MB Flash, 192KB SRAM, 80KB stack
```

## Make Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `WOLFSSL_ROOT` | `../../wolfssl` | wolfSSL source tree |
| `STM32CUBE_FW_F4` | `~/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3` | ST HAL pack |
| `STLINK_SERIAL` | (auto) | STLINK probe serial for multi-probe |
| `CONFIG` | `c` | Build variant: `c` or `hw` |
| `TARGET` | `test` | Entry point: `test` or `bench` |

## Notes

- The STM32F437 CRYP peripheral does not support 192-bit AES keys (`NO_AES_192`).
- The `hw` variant uses the wolfSSL STM32 CRYP/HASH port (`wolfcrypt/src/port/st/stm32.c`).
- Stack is set to 80 KB to accommodate ECC operations with `NO_WOLFSSL_SMALL_STACK`.
- UART output uses `__io_putchar` / `_write` retarget (newlib-nano).
