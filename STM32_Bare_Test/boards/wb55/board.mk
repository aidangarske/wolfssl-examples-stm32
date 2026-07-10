# P-NUCLEO-WB55: Cortex-M4F. AES + RNG + PKA (V1).
CUBE_FW_VAR     := STM32CUBE_FW_WB
CUBE_FW_DEFAULT := $(firstword $(wildcard $(HOME)/STM32Cube/Repository/STM32Cube_FW_WB_V*))
HAL_FAMILY      := WBxx
HAL_CONF_SHORT  := wb
OPENOCD_TARGET  := stm32wbx.cfg
b_system        := boards/wb55/system_stm32wbxx.c
b_chip          := wb55
b_core          := m4f
b_serial        := 0670FF393738425043094240
