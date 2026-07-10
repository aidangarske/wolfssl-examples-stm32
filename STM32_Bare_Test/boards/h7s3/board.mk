# NUCLEO-H7S3L8: Cortex-M7 (H7RS). Only 64 KB user flash -- SRAM-load via
# OpenOCD (lrun linker script), same approach as N657.
CUBE_FW_VAR     := STM32CUBE_FW_H7RS
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_H7RS_V1.3.0
HAL_FAMILY      := H7RSxx
HAL_CONF_SHORT  := h7rs
OPENOCD_TARGET  := stm32h7rsx.cfg
b_system        := boards/h7s3/system_stm32h7rsxx.c
b_chip          := h7s3
b_core          := m7dp
b_ldsuffix      := lrun
b_serial        := 003F00283133510F35333335
