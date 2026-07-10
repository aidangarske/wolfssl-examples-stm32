# NUCLEO-F303ZE: Cortex-M4F at 64 MHz. No HW crypto -- software (or thumb2
# asm under CONFIG=asm). Cortex-M4F software reference point.
CUBE_FW_VAR     := STM32CUBE_FW_F3
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_F3_V1.11.6
HAL_FAMILY      := F3xx
HAL_CONF_SHORT  := f3
OPENOCD_TARGET  := stm32f3x.cfg
b_system        := boards/f303/system_stm32f3xx.c
b_chip          := f303
b_chipdef       := STM32F303xE
b_core          := m4f
# Startup basename is xE not the usual xx (matches the device variant).
STARTUP_S       := boards/f303/startup_stm32f303xe.s
b_serial        := 0667FF515049657187205420
