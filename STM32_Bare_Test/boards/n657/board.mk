# NUCLEO-N657X0-Q: Cortex-M55 at 600 MHz. SAES + PKA (V2) + HASH + RNG.
# No internal flash -- SRAM-load via OpenOCD (lrun linker script).
CUBE_FW_VAR     := STM32CUBE_FW_N6
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_N6_V1.3.0
HAL_FAMILY      := N6xx
HAL_CONF_SHORT  := n6
OPENOCD_TARGET  := stm32n6x.cfg
b_system        := boards/n657/system_stm32n6xx.c
b_chip          := n657
b_core          := m55
b_ldsuffix      := lrun
b_serial        := 0046003C3433511830343835
