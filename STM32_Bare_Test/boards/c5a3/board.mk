# NUCLEO-C5A3 (STM32C5A3): Cortex-M33. AES + HASH + RNG + SAES + PKA (V2) + CCB.
# C5 ships ST's new-generation HAL ("HAL2"), a different driver generation than
# the classic HAL the other cubemx boards use. BUILD=cubemx uses the new-gen HAL
# for board bring-up only (clock/cache/NVIC); wolfcrypt stays on the register
# crypto path (WOLFSSL_STM32_BARE, selected via STM32_HAL_NEWGEN in
# user_settings.h) because the new-gen HAL has no classic CRYP/PKA/CCB/RNG driver
# APIs. BUILD=bare is pure direct-register. CubeIDE workspace DFP, C-file startup,
# STM32_Programmer_CLI flashing.
CUBE_FW_VAR     := STM32CUBE_FW_C5
CUBE_FW_DEFAULT := $(HOME)/STM32CubeIDE/workspace_2.0.0/STM32C5A3/STM32C5A3_cmake/stm32c5xx_dfp
STM32CUBE_FW_C5 ?= $(CUBE_FW_DEFAULT)
CUBE_DIR        := $(STM32CUBE_FW_C5)
CMSIS_DEVICE    := $(CUBE_DIR)/Include
OPENOCD_TARGET  := stm32c5x.cfg
b_chip          := c5a3
b_core          := m33f
STARTUP_S       :=
b_serial        := 003D00203235510F37333439
USE_CUBE_PROGRAMMER := 1
# NUCLEO-C5A3ZG soldered HSE crystal = 48 MHz. The new-gen HAL gates its clock
# enums (hal_rcc_hse_t etc.) on HSE_VALUE, normally force-included via the CMSIS
# RTE stm32_external_env.h; define it here so the mx_rcc init compiles. Harmless
# for the BARE build (no HAL enums).
b_extra_defs    := -DHSE_VALUE=48000000UL -DHSE_STARTUP_TIMEOUT=100UL

# Root of the C5 CubeIDE project tree (one level up from the DFP).
C5_ROOT := $(CUBE_DIR)/..

ifeq ($(BUILD),cubemx)
  # ---- New-generation HAL board bring-up ----
  HAL_FAMILY      := C5xx
  HAL_FAMILY_LC   := c5xx
  HAL_CONF_SHORT  := c5
  b_hal_newgen    := 1
  # C5's own CMSIS-Core (Cortex-M33) ships in the project tree.
  CMSIS_CORE      := $(C5_ROOT)/arch/cmsis/CMSIS/Core/Include
  # New-gen HAL/LL live under stm32c5xx_drivers/, not Drivers/STM32C5xx_HAL_Driver.
  HAL_SRC_DIR     := $(C5_ROOT)/stm32c5xx_drivers/hal
  HAL_INC_DIR     := $(C5_ROOT)/stm32c5xx_drivers/hal
  HAL_CONF_DIR    := $(C5_ROOT)/generated/hal
  HAL_EXTRA_INC   := -I$(C5_ROOT)/stm32c5xx_drivers/ll \
                     -I$(C5_ROOT)/stm32c5xx_drivers/timebases \
                     -I$(C5_ROOT)/generated/hal
  # Board-init HAL modules + the CubeMX-generated mx_* init (no crypto modules:
  # wolfcrypt drives the SAES/PKA/RNG/CCB via registers).
  HAL_C_SRC := \
    $(HAL_SRC_DIR)/stm32c5xx_hal.c \
    $(HAL_SRC_DIR)/stm32c5xx_hal_cortex.c \
    $(HAL_SRC_DIR)/stm32c5xx_hal_rcc.c \
    $(HAL_SRC_DIR)/stm32c5xx_hal_pwr.c \
    $(HAL_SRC_DIR)/stm32c5xx_hal_flash.c \
    $(HAL_SRC_DIR)/stm32c5xx_hal_flash_itf.c \
    $(HAL_SRC_DIR)/stm32c5xx_hal_icache.c \
    $(HAL_SRC_DIR)/stm32c5xx_hal_gpio.c \
    $(C5_ROOT)/user_modifiable/Device/STM32C5A3ZGT6/system_stm32c5xx.c \
    $(C5_ROOT)/generated/hal/mx_rcc.c \
    $(C5_ROOT)/generated/hal/mx_icache.c \
    $(C5_ROOT)/generated/hal/mx_cortex_nvic.c \
    $(C5_ROOT)/generated/hal/mx_cortex_mpu.c
  BOARD_C_SRC := boards/c5a3/startup_stm32c5a3xx.c boards/c5a3/hw_init_cubemx.c
else
  # ---- Pure bare-metal (direct register) ----
  # Borrow the Cortex-M33 CMSIS-Core from the U5 pack (no C5 pack ships it);
  # overridable via STM32CUBE_FW_U5, matching the u5xx family convention.
  STM32CUBE_FW_U5 ?= $(HOME)/STM32Cube/Repository/STM32Cube_FW_U5_V1.8.0
  CMSIS_CORE  ?= $(STM32CUBE_FW_U5)/Drivers/CMSIS/Core/Include
  BOARD_C_SRC := boards/c5a3/startup_stm32c5a3xx.c boards/c5a3/hw_init.c
endif
