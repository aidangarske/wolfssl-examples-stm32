# Shared atoms for the STM32G4 family (g491, g474).
CUBE_FW_VAR     := STM32CUBE_FW_G4
CUBE_FW_DEFAULT := $(firstword $(wildcard $(HOME)/STM32Cube/Repository/STM32Cube_FW_G4_V*))
HAL_FAMILY      := G4xx
HAL_CONF_SHORT  := g4
OPENOCD_TARGET  := stm32g4x.cfg
b_system        := boards/families/g4xx/system_stm32g4xx.c
