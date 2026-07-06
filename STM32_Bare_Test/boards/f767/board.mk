# NUCLEO-F767ZI: Cortex-M7 (single-precision FPU). CRYP + HASH + RNG.
CUBE_FW_VAR     := STM32CUBE_FW_F7
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_F7_V1.17.4
HAL_FAMILY      := F7xx
HAL_CONF_SHORT  := f7
OPENOCD_TARGET  := stm32f7x.cfg
b_system        := boards/f767/system_stm32f7xx.c
b_chip          := f767
b_core          := m7sp
b_serial        := 066AFF343433464757202234
