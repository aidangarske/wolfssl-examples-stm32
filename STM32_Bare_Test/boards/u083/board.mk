# NUCLEO-U083RC: Cortex-M0+. AES + RNG only (no SAES/HASH/PKA/CRYP).
CUBE_FW_VAR     := STM32CUBE_FW_U0
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_U0_V1.3.0
HAL_FAMILY      := U0xx
HAL_CONF_SHORT  := u0
OPENOCD_TARGET  := stm32u0x.cfg
b_system        := boards/u083/system_stm32u0xx.c
b_chip          := u083
b_core          := m0plus
b_hal_v2        := 1
b_serial        := 0670FF363445503043092050
