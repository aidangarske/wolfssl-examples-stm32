# NUCLEO-H7A3ZI-Q: Cortex-M7 (double-precision FPU). RNG only. Q-variant
# silicon (on-chip SMPS) needs the xxQ device header for PWR_CR3 fields.
include $(TOP)/boards/families/h7xx/family.mk
b_chip         := h7a3
b_core         := m7dp
b_chipdef      := STM32H7A3xxQ
HAL_CONF_SHORT := h7_rng
b_serial       := 003600473132511838363431
USE_CUBE_PROGRAMMER := 1
