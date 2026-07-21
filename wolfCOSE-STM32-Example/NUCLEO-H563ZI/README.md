# wolfCOSE Example - NUCLEO-H563ZI

Runs the wolfCOSE self test (a `COSE_Sign1` ES256 sign and verify) on the
NUCLEO-H563ZI and prints the result over the ST-LINK virtual COM port. Uses
software crypto (SP math) with entropy from the STM32 TRNG, so it does not
depend on the STM32 hardware hash accelerator.

## Prerequisites

- STM32CubeMX and STM32CubeIDE (or `make` with an arm-none-eabi toolchain).
- The wolfSSL pack (`I-CUBE-wolfSSL` 5.9.2 or later) and the wolfCOSE pack
  (`I-CUBE-wolfCOSE`), installed from
  [wolfssl.com/files/ide](https://www.wolfssl.com/files/ide/).

## Steps

1. **Install packs** in STM32CubeMX: `Help`, `Manage embedded software
   packages`, `From Local...`, install the wolfSSL pack, then the wolfCOSE pack.
2. **New project** for the NUCLEO-H563ZI.
3. **Enable peripherals** in the `.ioc`:
   - `USART3` in Asynchronous mode (this is the ST-LINK VCP, pins PD8/PD9) at
     115200 baud, for printf output.
   - `RNG` (the true random number generator), for signing entropy.
4. **Add components**: `Software Packs`, `Select Components`, enable
   `wolfSSL` `wolfCrypt` `Core`, and `wolfCOSE` `Core` and `Test`. Enable both
   packs in the `Software Packs` configuration category.
5. **Configuration**: point the wolfSSL pack at the provided `user_settings.h`
   (define `WOLFSSL_USER_SETTINGS` and put `user_settings.h` on the include
   path). It is the same software-crypto configuration the wolfIP H563ZI example
   uses, with ECC P-256 and SHA-256 for ES256.
6. **Generate code** (`Project`, `Generate Code`).
7. **Add the snippets below** to `main.c` in the matching `USER CODE` sections.
8. **Build**: `make GCC_PATH=/path/to/arm-none-eabi/bin`, or build in
   STM32CubeIDE.
9. **Flash and watch**: open the ST-LINK VCP at 115200 baud. Expected output:

   ```
   Running wolfCOSE test (COSE_Sign1 ES256)...
   wolfCOSE test: PASS (COSE_Sign1 99 bytes)
   ```

## main.c snippets

Entropy from the TRNG (`USER CODE BEGIN 0`):

```c
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
```

Run the test (`USER CODE BEGIN 2`, after the peripherals are initialized):

```c
extern int wolfCOSETest(void);
wolfCOSETest();
```

## Notes

- `wolfCOSETest()` comes from the wolfCOSE pack `Test` component
  (`wolfcose_test.c`); enabling that component is what builds it.
- This configuration uses no STM32 hardware crypto, so the STM32H5 hardware
  hash accelerator is not involved. Entropy comes from the TRNG through
  `custom_rand_gen_block`.
