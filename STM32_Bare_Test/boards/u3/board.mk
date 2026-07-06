# NUCLEO-U385RG-Q: Cortex-M33. SAES + PKA (V2) + DHUK.
CUBE_FW_VAR     := STM32CUBE_FW_U3
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_U3_V1.3.0
HAL_FAMILY      := U3xx
HAL_CONF_SHORT  := u3
OPENOCD_TARGET  := stm32u3x.cfg
b_system        := boards/u3/system_stm32u3xx.c
b_chip          := u385
b_core          := m33f
b_serial        := 0034004C3333511631363730
