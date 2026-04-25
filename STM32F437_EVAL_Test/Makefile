# ---------------------------------------------------------------------------
# STM32F437 demo — wolfCrypt test / bench
#
# Board: STM32439I-EVAL (G-EVAL) with STM32F437IIHx
# UART:  UART4 on PC10/PC11, 115200 8N1
# Clock: HSI 16 MHz -> PLL -> 160 MHz
#
# Usage:
#   make CONFIG=c    test       # wolfCrypt test, pure C SP math
#   make CONFIG=hw   test       # wolfCrypt test, STM32 HW HASH/CRYP
#   make CONFIG=c    bench      # wolfCrypt benchmark
#   make CONFIG=hw   bench      # wolfCrypt benchmark with HW crypto
#   make size-report            # aggregate size across all built configs
#   make flash                  # OpenOCD flash
#
# Make/env variables:
#   WOLFSSL_ROOT      = ../../wolfssl             (wolfSSL source tree)
#   STM32CUBE_FW_F4   = $(HOME)/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3
#   STLINK_SERIAL     = (optional STLINK serial for multi-probe setups)
# ---------------------------------------------------------------------------

# ----- Toolchain -----------------------------------------------------------
CROSS        ?= arm-none-eabi-
CC            = $(CROSS)gcc
AS            = $(CROSS)gcc -x assembler-with-cpp
OBJCOPY       = $(CROSS)objcopy
SIZE          = $(CROSS)size

# ----- Repos / paths -------------------------------------------------------
TOP          := $(abspath .)
# Repo layout: .../GitHub/wolfssl-examples-stm32/STM32F437_EVAL_Test/
# Siblings (e.g. wolfssl) live at .../GitHub/<repo> — 2 levels up from $(TOP).
REPO_SIBLING := $(abspath $(TOP)/../..)

WOLFSSL_ROOT    ?= $(REPO_SIBLING)/wolfssl
STM32CUBE_FW_F4 ?= $(HOME)/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3

# ----- Target / build config -----------------------------------------------
TARGET   ?= test
CONFIG   ?= c

ifeq ($(filter $(TARGET),test bench),)
  $(error TARGET must be one of: test | bench)
endif
ifeq ($(filter $(CONFIG),c hw),)
  $(error CONFIG must be one of: c | hw)
endif

BUILD_DIR := $(TOP)/build/$(TARGET)-$(CONFIG)

# ----- MCU / arch flags ----------------------------------------------------
MCU_FLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CPU_DEFS  := -DSTM32F437xx -DUSE_HAL_DRIVER

# ----- Variant overlays ----------------------------------------------------
VARIANT_DEFS :=
ifeq ($(CONFIG),c)
  VARIANT_DEFS += -DBUILD_CONFIG_NAME=\"c\"
endif
ifeq ($(CONFIG),hw)
  VARIANT_DEFS += -DBUILD_CONFIG_NAME=\"hw\" \
                  -UNO_STM32_HASH -UNO_STM32_CRYPTO \
                  -DSTM32_HASH -DSTM32_CRYPTO
endif

# ----- Include paths -------------------------------------------------------
INCLUDES := \
  -I$(TOP) \
  -I$(TOP)/src \
  -I$(STM32CUBE_FW_F4)/Drivers/CMSIS/Core/Include \
  -I$(STM32CUBE_FW_F4)/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
  -I$(STM32CUBE_FW_F4)/Drivers/STM32F4xx_HAL_Driver/Inc \
  -I$(STM32CUBE_FW_F4)/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy \
  -I$(WOLFSSL_ROOT)

# ----- Compile / link flags ------------------------------------------------
COMMON_DEFS := -DWOLFSSL_USER_SETTINGS $(CPU_DEFS) $(VARIANT_DEFS)

# -O2 + -fomit-frame-pointer required for Thumb2 inline ASM register allocation
CFLAGS    = $(MCU_FLAGS) $(INCLUDES) $(COMMON_DEFS) \
            -O2 -fomit-frame-pointer \
            -ffunction-sections -fdata-sections \
            -Wall -Wno-unused-function -Wno-unused-variable \
            -fno-common -fno-strict-aliasing \
            -MMD -MP -std=gnu11

ASFLAGS  := $(MCU_FLAGS) -c -g3

LDSCRIPT := $(TOP)/linker/stm32f437_flat.ld
LDFLAGS  := $(MCU_FLAGS) \
            -T$(LDSCRIPT) \
            --specs=nosys.specs --specs=nano.specs \
            -Wl,--gc-sections \
            -Wl,-Map=$(BUILD_DIR)/app.map \
            -Wl,--print-memory-usage

LDLIBS   := -lc -lm

# ----- Sources -------------------------------------------------------------
# Application / glue
SRC_C  := \
  src/main_$(TARGET).c \
  src/hw_init.c \
  src/stubs.c \
  src/system_stm32f4xx.c \
  src/stm32f4xx_it.c

SRC_ASM := src/startup_stm32f437xx.s

# STM32Cube HAL — compile only modules enabled in hal_conf.h
HAL_SRC_DIR := $(STM32CUBE_FW_F4)/Drivers/STM32F4xx_HAL_Driver/Src
HAL_SOURCES := \
  stm32f4xx_hal.c \
  stm32f4xx_hal_cortex.c \
  stm32f4xx_hal_dma.c \
  stm32f4xx_hal_dma_ex.c \
  stm32f4xx_hal_flash.c \
  stm32f4xx_hal_flash_ex.c \
  stm32f4xx_hal_flash_ramfunc.c \
  stm32f4xx_hal_gpio.c \
  stm32f4xx_hal_pwr.c \
  stm32f4xx_hal_pwr_ex.c \
  stm32f4xx_hal_rcc.c \
  stm32f4xx_hal_rcc_ex.c \
  stm32f4xx_hal_rng.c \
  stm32f4xx_hal_uart.c

# HW crypto variant adds CRYP + HASH drivers
ifneq ($(filter hw,$(CONFIG)),)
HAL_SOURCES += \
  stm32f4xx_hal_cryp.c \
  stm32f4xx_hal_cryp_ex.c \
  stm32f4xx_hal_hash.c \
  stm32f4xx_hal_hash_ex.c
endif

HAL_SRC := $(addprefix $(HAL_SRC_DIR)/,$(HAL_SOURCES))

# wolfCrypt — compile the whole wolfcrypt/src tree; user_settings gates out
# what isn't needed.
WC_SRC_DIR := $(WOLFSSL_ROOT)/wolfcrypt/src
WC_ALL     := $(wildcard $(WC_SRC_DIR)/*.c)
# Always exclude these
WC_EXCLUDE := \
  $(WC_SRC_DIR)/evp.c \
  $(WC_SRC_DIR)/misc.c \
  $(WC_SRC_DIR)/selftest.c \
  $(WC_SRC_DIR)/async.c \
  $(WC_SRC_DIR)/wolfcrypt_first.c \
  $(WC_SRC_DIR)/wolfcrypt_last.c \
  $(WC_SRC_DIR)/fips.c \
  $(WC_SRC_DIR)/fips_test.c

WC_SRC := $(filter-out $(WC_EXCLUDE),$(WC_ALL)) \
          $(WC_SRC_DIR)/port/st/stm32.c

# Thumb2 inline assembly sources (ChaCha, Poly1305, Curve25519, SHA-512, AES)
WC_SRC += \
  $(WC_SRC_DIR)/port/arm/thumb2-chacha-asm_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-poly1305-asm_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-curve25519_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-sha512-asm_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-aes-asm_c.c \
  $(WC_SRC_DIR)/port/arm/thumb2-sha256-asm_c.c

# Target entry point
ifeq ($(TARGET),test)
  SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/test/test.c
endif
ifeq ($(TARGET),bench)
  SRC_C += $(WOLFSSL_ROOT)/wolfcrypt/benchmark/benchmark.c
endif

ALL_C_SRC := $(SRC_C) $(WC_SRC) $(HAL_SRC)

# ----- Object files --------------------------------------------------------
rel_obj = $(BUILD_DIR)/obj/$(subst /,__,$(patsubst /%,%,$(1))).o

C_OBJ   := $(foreach s,$(ALL_C_SRC),$(call rel_obj,$(s)))
ASM_OBJ := $(foreach s,$(SRC_ASM),$(call rel_obj,$(s)))
OBJS    := $(C_OBJ) $(ASM_OBJ)
DEPS    := $(OBJS:.o=.d)

# ----- Default target -------------------------------------------------------
.PHONY: all test bench build flash size clean distclean size-report

all: test

test:
	$(MAKE) build TARGET=test

bench:
	$(MAKE) build TARGET=bench

build: $(BUILD_DIR)/app.bin $(BUILD_DIR)/app.hex size

# ----- Rules ----------------------------------------------------------------
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)/obj

define COMPILE_C_TEMPLATE
$(call rel_obj,$(1)): $(1) | $(BUILD_DIR)
	@echo "  CC   $$<"
	@mkdir -p $$(@D)
	@$(CC) $(CFLAGS) -c $$< -o $$@
endef

define COMPILE_ASM_TEMPLATE
$(call rel_obj,$(1)): $(1) | $(BUILD_DIR)
	@echo "  AS   $$<"
	@mkdir -p $$(@D)
	@$(AS) $(ASFLAGS) -c $$< -o $$@
endef

$(foreach s,$(ALL_C_SRC),$(eval $(call COMPILE_C_TEMPLATE,$(s))))
$(foreach s,$(SRC_ASM),$(eval $(call COMPILE_ASM_TEMPLATE,$(s))))

$(BUILD_DIR)/app.elf: $(OBJS) $(LDSCRIPT)
	@echo "  LD   $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD_DIR)/app.bin: $(BUILD_DIR)/app.elf
	@echo "  BIN  $@"
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/app.hex: $(BUILD_DIR)/app.elf
	@$(OBJCOPY) -O ihex $< $@

size: $(BUILD_DIR)/app.elf
	@echo
	@echo "----- size: $(TARGET)-$(CONFIG) -----"
	@$(SIZE) $<

flash: $(BUILD_DIR)/app.elf
ifdef STLINK_SERIAL
	openocd -f interface/stlink.cfg \
	    -c "adapter serial $(STLINK_SERIAL)" \
	    -f target/stm32f4x.cfg \
	    -c "program $< verify reset exit"
else
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
	    -c "program $< verify reset exit"
endif

size-report:
	@echo "== STM32F437_EVAL_Test size report =="
	@for d in build/*; do \
	  if [ -f $$d/app.elf ]; then \
	    echo "--- $$d ---"; \
	    $(SIZE) $$d/app.elf; \
	  fi \
	done

clean:
	rm -rf build/$(TARGET)-$(CONFIG)

distclean:
	rm -rf build

# ----- Dependency files -----------------------------------------------------
-include $(DEPS)
