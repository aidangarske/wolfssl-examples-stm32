# STM32F437I-EVAL: Cortex-M4F. CRYP + HASH + RNG. UART4 PC10/PC11 AF8 needs
# an external USB-serial adapter (not on the ST-LINK VCP).
include $(TOP)/boards/families/f4xx/family.mk
b_chip   := f437
b_core   := m4f
b_hal_v2 := 1
b_serial := 50FF70064977535534290587
