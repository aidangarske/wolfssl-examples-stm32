# NUCLEO-C562RE: Cortex-M33 at 144 MHz. AES + HASH + RNG + SAES + PKA (V2).
# C5 firmware comes from the CubeIDE workspace DFP (no STM32Cube_FW_C5 pack
# exists yet); CMSIS-Core is borrowed from the U5 pack. BARE-only (no HAL
# wiring), C-file startup. stm32c5x.cfg isn't in upstream OpenOCD -- use
# STM32_Programmer_CLI.
CUBE_FW_VAR     := STM32CUBE_FW_C5
CUBE_FW_DEFAULT := $(HOME)/STM32CubeIDE/workspace_2.0.0/STM32C5A3/STM32C5A3_cmake/stm32c5xx_dfp
STM32CUBE_FW_C5 ?= $(CUBE_FW_DEFAULT)
CUBE_DIR        := $(STM32CUBE_FW_C5)
CMSIS_DEVICE    := $(CUBE_DIR)/Include
STM32CUBE_FW_U5 ?= $(HOME)/STM32Cube/Repository/STM32Cube_FW_U5_V1.8.0
CMSIS_CORE      ?= $(STM32CUBE_FW_U5)/Drivers/CMSIS/Core/Include
OPENOCD_TARGET  := stm32c5x.cfg
b_chip          := c562
b_core          := m33f
STARTUP_S       :=
BOARD_C_SRC     := boards/c562/startup_stm32c562xx.c boards/c562/hw_init.c
b_serial        := 003900473335510535383531
USE_CUBE_PROGRAMMER := 1
