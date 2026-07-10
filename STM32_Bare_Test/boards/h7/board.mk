# NUCLEO-H753ZI: Cortex-M7 (double-precision FPU). CRYP + HASH + RNG.
include $(TOP)/boards/families/h7xx/family.mk
b_chip   := h753
b_core   := m7dp
b_serial := 002900373431511237393330
# Dual-bank H7: upstream OpenOCD flash read fails -- use CubeProgrammer.
USE_CUBE_PROGRAMMER := 1
