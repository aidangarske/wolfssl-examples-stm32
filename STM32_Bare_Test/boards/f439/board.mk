# STM32F439: Cortex-M4F. CRYP + HASH + RNG. F4 HAL v1.28+ unified CRYP API
# (STM32_HAL_V2) -- drop b_hal_v2 if pinning a pre-v1.28 pack.
include $(TOP)/boards/families/f4xx/family.mk
b_chip   := f439
b_core   := m4f
b_hal_v2 := 1
b_serial := 0671FF555380535067041841
