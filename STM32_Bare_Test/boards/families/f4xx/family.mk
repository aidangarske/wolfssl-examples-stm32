# Shared atoms for the STM32F4 family (f437, f439).
CUBE_FW_VAR     := STM32CUBE_FW_F4
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3
HAL_FAMILY      := F4xx
HAL_CONF_SHORT  := f4
OPENOCD_TARGET  := stm32f4x.cfg
b_system        := boards/families/f4xx/system_stm32f4xx.c
