# NUCLEO-F207ZG: Cortex-M3 (no FPU) at 16 MHz. RNG only (no CRYP/HASH on
# F207). USART3 PD8/PD9 AF7 on the ST-LINK VCP.
CUBE_FW_VAR     := STM32CUBE_FW_F2
CUBE_FW_DEFAULT := $(HOME)/STM32Cube/Repository/STM32Cube_FW_F2_V1.9.6
HAL_FAMILY      := F2xx
HAL_CONF_SHORT  := f2
OPENOCD_TARGET  := stm32f2x.cfg
b_system        := boards/f207/system_stm32f2xx.c
b_chip          := f207
b_core          := m3soft
b_serial        := 066DFF495772784967094756
