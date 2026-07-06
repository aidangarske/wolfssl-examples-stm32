# Shared atoms for the STM32H5 family (h5 = H563, h573 = H573).
CUBE_FW_VAR     := STM32CUBE_FW_H5
CUBE_FW_DEFAULT := $(firstword $(wildcard $(HOME)/STM32Cube/Repository/STM32Cube_FW_H5_V*))
HAL_FAMILY      := H5xx
HAL_CONF_SHORT  := h5
OPENOCD_TARGET  := stm32h5x.cfg
b_system        := boards/families/h5xx/system_stm32h5xx.c
