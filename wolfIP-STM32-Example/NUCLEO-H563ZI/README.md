# wolfIP Example - NUCLEO-H563ZI

Pre-configured STM32CubeMX project for wolfIP on NUCLEO-H563ZI.

## Usage

1. Install the wolfIP pack (see parent directory README)
2. Open `NUCLEO-H563ZI-wolfIP.ioc` in STM32CubeMX
3. Generate code (Project -> Generate Code)
4. Add wolfIP code to main.c USER CODE sections (see parent README)
5. Build: `make GCC_PATH=/path/to/arm-toolchain/bin`
6. Flash and ping: `ping 192.168.0.200`

## What's Pre-Configured

- ETH: RMII mode with correct GPIO pins
- NVIC: ETH global interrupt enabled
- Software Packs: wolfIP Core and Eth components
