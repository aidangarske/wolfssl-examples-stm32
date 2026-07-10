# NUCLEO-G071RB: Cortex-M0+ at 16 MHz. No HW crypto at all (software DRBG).
# USART2 PA2/PA3 AF1 on the ST-LINK VCP.
CUBE_FW_VAR     := STM32CUBE_FW_G0
CUBE_FW_DEFAULT := $(firstword $(wildcard $(HOME)/STM32Cube/Repository/STM32Cube_FW_G0_V*))
HAL_FAMILY      := G0xx
HAL_CONF_SHORT  := g0
OPENOCD_TARGET  := stm32g0x.cfg
b_system        := boards/g071/system_stm32g0xx.c
b_chip          := g071
b_core          := m0plus_soft
b_serial        := 0668FF505051717867183933
