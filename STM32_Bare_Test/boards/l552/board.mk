# NUCLEO-L552ZE-Q: Cortex-M33 + FPU. TrustZone disabled at option-byte
# level for this BARE build (-mcmse so headers resolve secure aliases).
include $(TOP)/boards/families/l5xx/family.mk
b_chip   := l552
b_core   := m33f_cmse
b_serial := 0669FF505352716587244234
