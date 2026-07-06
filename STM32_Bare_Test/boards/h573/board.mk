# NUCLEO-H573ZI: Cortex-M33 HSI 64 MHz. FULL crypto (AES+HASH+RNG+SAES+PKA
# +DHUK). Same NUCLEO-144 PCB / probe as H563ZI.
include $(TOP)/boards/families/h5xx/family.mk
b_chip   := h573
b_core   := m33f
b_serial := 005000313132511438363431
USE_CUBE_PROGRAMMER := 1
