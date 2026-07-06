# Shared atoms for the STM32U5 family (u5 = U575, u585, u545).
CUBE_FW_VAR     := STM32CUBE_FW_U5
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_U5_V1.8.0
HAL_FAMILY      := U5xx
HAL_CONF_SHORT  := u5
OPENOCD_TARGET  := stm32u5x.cfg
b_system        := boards/families/u5xx/system_stm32u5xx.c
