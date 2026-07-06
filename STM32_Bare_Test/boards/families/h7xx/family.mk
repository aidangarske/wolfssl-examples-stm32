# Shared atoms for the STM32H7 family (h7 = H753, h723, h7a3).
# h723/h7a3 override HAL_CONF_SHORT to h7_rng (RNG-only conf).
CUBE_FW_VAR     := STM32CUBE_FW_H7
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_H7_V1.13.0
HAL_FAMILY      := H7xx
HAL_CONF_SHORT  := h7
OPENOCD_TARGET  := stm32h7x.cfg
b_system        := boards/families/h7xx/system_stm32h7xx.c
