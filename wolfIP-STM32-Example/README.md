# wolfIP STM32 CubeMX Example

This directory contains the wolfIP CMSIS pack for STM32CubeMX. The pack provides a portable TCP/IP stack that works on any STM32 microcontroller with Ethernet.

## Supported Boards

Any STM32 with Ethernet peripheral:
- STM32F4 series (F407, F429, F439, F469, etc.)
- STM32F7 series (F746, F767, F769, etc.)
- STM32H5 series (H563, H573, etc.)
- STM32H7 series (H743, H753, H747, etc.)

## Quick Start

### Step 1: Install the Pack

1. Open STM32CubeMX
2. Go to **Help → Manage Embedded Software Packages**
3. Click **From Local...**
4. Select `wolfSSL.I-CUBE-wolfIP.1.0.0.pack` from this directory
   - Or download the latest from [wolfSSL](https://www.wolfssl.com/files/ide/I-CUBE-wolfIP.pack)
5. Accept the license agreement

### Step 2: Create Your Project

1. Create a new project for your STM32 board (or use a pre-configured .ioc from the board-specific subdirectory)
2. Configure **Connectivity → ETH**:
   - Mode: **RMII** (most common) or MII
   - Verify GPIO pin assignments match your board's PHY (CubeMX auto-configures for NUCLEO boards)
3. Configure **System Core → NVIC**:
   - Enable **ETH global interrupt** (CRITICAL - required for RX!)
4. Configure **Software Packs → Select Components**:
   - Expand wolfSSL.I-CUBE-wolfIP
   - Check **Core** (required)
   - Check **Eth** (required for HAL driver)
   - Check **HTTP** (optional - embedded web server)
5. Click **Generate Code** (Makefile recommended for command-line builds)

**Note:** For NUCLEO boards, CubeMX auto-configures the correct ETH GPIO pins. For custom boards, verify pins match your PHY datasheet.

### Step 3: Add wolfIP Code

Add this to your `main.c` in the USER CODE sections:

```c
/* USER CODE BEGIN Includes */
#include "wolfip.h"
#include "stm32_hal_eth.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
static struct wolfIP *ipstack = NULL;

/* Adjust IP for your network */
#define MY_IP      atoip4("192.168.0.200")
#define MY_MASK    atoip4("255.255.255.0")
#define MY_GATEWAY atoip4("192.168.0.1")
/* USER CODE END PV */
```

In main() after MX_ETH_Init():
```c
/* USER CODE BEGIN 2 */
wolfIP_init_static(&ipstack);
if (stm32_hal_eth_init(wolfIP_getdev(ipstack)) != 0) {
    Error_Handler();
}
wolfIP_ipconfig_set(ipstack, MY_IP, MY_MASK, MY_GATEWAY);
/* USER CODE END 2 */
```

In the main loop:
```c
/* USER CODE BEGIN 3 */
wolfIP_poll(ipstack, HAL_GetTick());
/* USER CODE END 3 */
```

Add required callbacks:
```c
/* USER CODE BEGIN 4 */
uint64_t wolfip_get_time_ms(void)
{
    return (uint64_t)HAL_GetTick();
}

uint32_t wolfIP_getrandom(void)
{
    static uint32_t seed = 12345;
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) ^ HAL_GetTick();
}
/* USER CODE END 4 */
```

### Step 4: Build and Test

1. Build your project
2. Flash to your board
3. Connect Ethernet cable
4. From your PC: `ping 192.168.0.200`

## Troubleshooting

### No ping response
- Verify ETH global interrupt is ENABLED in CubeMX NVIC
- Check Ethernet cable and link LED
- Verify IP is on same subnet as your PC

### Build errors about missing headers
- Ensure both **Core** and **Eth** components are checked in Software Packs

### stm32_hal_eth_init returns error
- Check GPIO pins match your board's PHY
- Verify ETH is configured correctly in CubeMX

## Features

- **Zero dynamic memory** - All buffers pre-allocated
- **Multi-family support** - Works on F4, F7, H5, H7
- **Auto-configuration** - RMII/MII interface auto-detected
- **Optional TLS** - Integrates with wolfSSL for secure connections
- **HTTP server** - Built-in lightweight web server

## Resources

- [wolfIP GitHub](https://github.com/wolfSSL/wolfip)
- [wolfIP Documentation](https://github.com/wolfSSL/wolfip/tree/master/IDE/STM32Cube)
- [wolfSSL Support](mailto:support@wolfssl.com)

## License

wolfIP is provided under GPLv2 or a commercial license from wolfSSL Inc.
