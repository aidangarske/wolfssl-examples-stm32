# NUCLEO-C031C6: Cortex-M0+ at 12 MHz. 32 KB flash (smallest). No HW
# crypto. USART2 PA2/PA3 AF1 on the ST-LINK VCP.
CUBE_FW_VAR     := STM32CUBE_FW_C0
CUBE_FW_DEFAULT := $(firstword $(wildcard $(HOME)/STM32Cube/Repository/STM32Cube_FW_C0_V*))
HAL_FAMILY      := C0xx
HAL_CONF_SHORT  := c0
OPENOCD_TARGET  := stm32c0x.cfg
b_system        := boards/c031/system_stm32c0xx.c
b_chip          := c031
b_core          := m0plus_soft
b_serial        := 066CFF505448826687073124
