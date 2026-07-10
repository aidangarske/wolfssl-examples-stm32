# NUCLEO-WL55JC: Cortex-M4 (no FPU, cost-reduced). AES + RNG + PKA (V1).
CUBE_FW_VAR     := STM32CUBE_FW_WL
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_WL_V1.5.0
HAL_FAMILY      := WLxx
HAL_CONF_SHORT  := wl
OPENOCD_TARGET  := stm32wlx.cfg
b_system        := boards/wl55/system_stm32wlxx.c
b_chip          := wl55
b_core          := m4soft
b_extra_defs    := -DCORE_CM4
STARTUP_S       := boards/wl55/startup_stm32wl55xx_cm4.s
b_serial        := 002300113756501520303658
