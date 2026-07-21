# wolfCOSE STM32 CubeMX Example

This directory contains a pre-configured STM32CubeMX project that uses the wolfCOSE CMSIS pack, a zero-allocation CBOR (RFC 8949) and COSE (RFC 9052/9053) library built on wolfCrypt. It runs a COSE_Sign1 ES256 self test on the device and prints the result over UART.

The wolfCOSE CMSIS pack is hosted at wolfSSL: <https://www.wolfssl.com/files/ide/I-CUBE-wolfCOSE.pack>
It depends on the wolfSSL pack: <https://www.wolfssl.com/files/ide/I-CUBE-wolfSSL.pack>

## Supported Boards

Any STM32 with a UART (for test output) and RNG (for signing entropy):
- STM32F4 series (F407, F429, F439, F469, etc.)
- STM32F7 series (F746, F767, F769, etc.)
- STM32H5 series (H563, H573, etc.)
- STM32H7 series (H743, H753, H747, etc.)

## Quick Start

### Step 1: Install the Packs

1. Download the wolfSSL pack from <https://www.wolfssl.com/files/ide/I-CUBE-wolfSSL.pack> and the wolfCOSE pack from <https://www.wolfssl.com/files/ide/I-CUBE-wolfCOSE.pack>
2. Open STM32CubeMX
3. Go to **Help -> Manage Embedded Software Packages**
4. Click **From Local...** and install the wolfSSL pack, then the wolfCOSE pack
5. Accept the license agreements

### Step 2: Create Your Project

1. Create a new project for your STM32 board (or use a pre-configured .ioc from the board-specific subdirectory)
2. Configure **Connectivity -> USART3** (the ST-LINK virtual COM port on NUCLEO boards):
   - Mode: **Asynchronous**, 115200 baud (printf output)
3. Configure **Security -> RNG**:
   - Enable the true random number generator (CRITICAL - signing entropy)
4. Configure **Software Packs -> Select Components**:
   - Expand wolfSSL.I-CUBE-wolfSSL, check **wolfCrypt Core** (required)
   - Expand wolfSSL.I-CUBE-wolfCOSE, check **Core** (required) and **Test** (the self test)
5. Click **Generate Code** (Makefile recommended for command-line builds)

**Note:** This example uses software crypto (SP math) with entropy from the RNG peripheral, so the STM32 hardware hash accelerator is not involved. Use the provided `NUCLEO-H563ZI/user_settings.h` for a known-good software configuration (ECC P-256 + SHA-256 for ES256).

### Step 3: Add wolfCOSE Code

Add the callbacks to your `main.c` (entropy from the TRNG, printf over USART3):

```c
/* USER CODE BEGIN 0 */
/* wolfSSL user_settings.h sets CUSTOM_RAND_GENERATE_BLOCK to this. */
int custom_rand_gen_block(unsigned char* output, unsigned int sz)
{
    extern RNG_HandleTypeDef hrng;
    uint32_t rnd;
    unsigned int chunk;
    unsigned int i = 0;

    while (i < sz) {
        if (HAL_RNG_GenerateRandomNumber(&hrng, &rnd) != HAL_OK) {
            return -1;
        }
        chunk = (sz - i < 4u) ? (sz - i) : 4u;
        memcpy(output + i, &rnd, chunk);
        i += chunk;
    }
    return 0;
}

/* Retarget printf to USART3 (ST-LINK VCP). */
int __io_putchar(int ch)
{
    extern UART_HandleTypeDef huart3;
    (void)HAL_UART_Transmit(&huart3, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
/* USER CODE END 0 */
```

In main() after the peripherals are initialized:
```c
/* USER CODE BEGIN 2 */
extern int wolfCOSETest(void);
wolfCOSETest();
/* USER CODE END 2 */
```

### Step 4: Build and Test

1. Build your project
2. Flash to your board
3. Open the ST-LINK virtual COM port at 115200 baud
4. Expected output:

```
Running wolfCOSE test (COSE_Sign1 ES256)...
wolfCOSE test: PASS (COSE_Sign1 99 bytes)
```

## Troubleshooting

### No UART output
- Verify USART3 pins/baud and the `__io_putchar` retarget
- Confirm the terminal is on the ST-LINK VCP at 115200 baud

### Build errors about missing headers
- Ensure both **wolfCrypt Core** and **wolfCOSE Core** are checked in Software Packs

### RNG failure or sign returns nonzero
- Enable the RNG peripheral in CubeMX and wire `custom_rand_gen_block` to it (`hrng` must be initialized)

### math.h not found (command-line build)
- Build with the STM32CubeIDE toolchain (`make GCC_PATH=<CubeIDE tools/bin>`); a bare arm-none-eabi-gcc without newlib fails here

## Features

- **Zero dynamic memory** - all operations run on caller-provided buffers
- **Post-quantum ready** - ES256 by default, ML-DSA available (RFC 9964)
- **Software crypto** - portable SP math, no STM32 hardware hash dependency
- **Self test** - one-call `wolfCOSETest()` from the pack Test component

## Resources

- [wolfCOSE GitHub](https://github.com/wolfSSL/wolfCOSE)
- [wolfCOSE STM32Cube Guide](https://github.com/wolfSSL/wolfCOSE/wiki/STM32Cube)
- [wolfSSL Support](mailto:support@wolfssl.com)

## License

wolfCOSE is provided under GPLv3 or a commercial license from wolfSSL Inc.
