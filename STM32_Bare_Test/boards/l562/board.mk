# STM32L562E-DK: Cortex-M33 + FPU. Bench unit ships with TZEN already
# disabled (no option-byte regression needed).
include $(TOP)/boards/families/l5xx/family.mk
b_chip   := l562
b_core   := m33f_cmse
b_serial := 002100243137511533333639
