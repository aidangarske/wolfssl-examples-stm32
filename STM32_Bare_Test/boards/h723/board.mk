# NUCLEO-H723ZG: Cortex-M7 (single-precision FPU). RNG only (H72x has no
# CRYP/HASH). Uses the RNG-only HAL conf.
include $(TOP)/boards/families/h7xx/family.mk
b_chip         := h723
b_core         := m7sp
HAL_CONF_SHORT := h7_rng
b_serial       := 002D003B544B500420343637
USE_CUBE_PROGRAMMER := 1
