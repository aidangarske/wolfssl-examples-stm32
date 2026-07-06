# NUCLEO-H563ZI: Cortex-M33 HSI 64 MHz. HASH + RNG only (no AES; PKA is
# present but ECDSA sign/verify hit a silicon abort, see user_settings.h).
include $(TOP)/boards/families/h5xx/family.mk
b_chip   := h563
b_core   := m33f
b_serial := 000D001E4D4B500C20373831
# stm32h5x flash driver isn't in upstream OpenOCD yet -- use CubeProgrammer.
USE_CUBE_PROGRAMMER := 1
