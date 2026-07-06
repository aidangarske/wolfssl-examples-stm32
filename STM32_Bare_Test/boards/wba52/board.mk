# NUCLEO-WBA52CG: Cortex-M33. AES + RNG + PKA (V2).
CUBE_FW_VAR     := STM32CUBE_FW_WBA
CUBE_FW_DEFAULT := $(firstword $(wildcard $(HOME)/STM32Cube/Repository/STM32Cube_FW_WBA_V*))
HAL_FAMILY      := WBAxx
HAL_CONF_SHORT  := wba
OPENOCD_TARGET  := stm32wba5x.cfg
b_system        := boards/wba52/system_stm32wbaxx.c
b_chip          := wba52
b_core          := m33f
b_serial        := 002600254D4B500820373831
