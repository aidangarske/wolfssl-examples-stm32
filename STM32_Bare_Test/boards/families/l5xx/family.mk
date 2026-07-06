# Shared atoms for the STM32L5 family (l552, l562).
CUBE_FW_VAR     := STM32CUBE_FW_L5
CUBE_FW_DEFAULT := $(firstword $(wildcard $(HOME)/STM32Cube/Repository/STM32Cube_FW_L5_V*))
HAL_FAMILY      := L5xx
HAL_CONF_SHORT  := l5
OPENOCD_TARGET  := stm32l5x.cfg
b_system        := boards/families/l5xx/system_stm32l5xx.c
