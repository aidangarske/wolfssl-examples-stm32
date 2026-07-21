# wolfCOSE Example - NUCLEO-H563ZI

Pre-configured STM32CubeMX project for wolfCOSE on NUCLEO-H563ZI.

## Usage

1. Install the wolfSSL and wolfCOSE packs (see parent directory README)
2. Open `NUCLEO-H563ZI-wolfCOSE.ioc` in STM32CubeMX
3. Generate code (Project -> Generate Code)
4. Add wolfCOSE code to main.c USER CODE sections (see parent README)
5. Build: `make GCC_PATH=/path/to/arm-toolchain/bin`
6. Flash and check UART (115200 baud): `wolfCOSE test: PASS`

## What's Pre-Configured

- USART3: ST-LINK VCP, Asynchronous at 115200 baud (printf output)
- RNG: true random number generator enabled (signing entropy)
- Software Packs: wolfSSL wolfCrypt Core, wolfCOSE Core and Test components
