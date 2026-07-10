# NUCLEO-L4A6ZG: Cortex-M4F at 16 MHz. AES + HASH + RNG. LPUART1 PG7/PG8
# AF8 (VddIO2 domain -- needs PWR.CR2.IOSV).
CUBE_FW_VAR     := STM32CUBE_FW_L4
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2
HAL_FAMILY      := L4xx
HAL_CONF_SHORT  := l4
OPENOCD_TARGET  := stm32l4x.cfg
b_system        := boards/l4a6/system_stm32l4xx.c
b_chip          := l4a6
b_core          := m4f
b_serial        := 0672FF535155878281164244
