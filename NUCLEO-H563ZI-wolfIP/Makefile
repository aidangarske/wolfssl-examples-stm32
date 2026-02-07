# Makefile for STM32H563ZI wolfIP Example
# NUCLEO-H563ZI board

######################################
# Target
######################################
TARGET = NUCLEO-H563ZI-wolfIP

######################################
# Building variables
######################################
DEBUG = 1
OPT = -Og

#######################################
# Paths
#######################################
BUILD_DIR = build

######################################
# Source files
######################################
# Core sources
C_SOURCES =  \
Core/Src/main.c \
Core/Src/stm32h5xx_it.c \
Core/Src/stm32h5xx_hal_msp.c \
Core/Src/system_stm32h5xx.c \
Core/Src/stm32h5_eth.c \
Core/Src/sysmem.c \
Core/Src/syscalls.c

# HAL sources
C_SOURCES += \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_cortex.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc_ex.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr_ex.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gpio.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_exti.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_eth.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_eth_ex.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma.c \
Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma_ex.c

# wolfIP sources
C_SOURCES += \
Middlewares/Third_Party/wolfSSL_wolfIP_wolfIP/wolfip/src/wolfip.c

# ASM sources
ASM_SOURCES = \
Drivers/CMSIS/Device/ST/STM32H5xx/Source/Templates/gcc/startup_stm32h563xx.s

#######################################
# Binaries
#######################################
# ARM GNU Toolchain prefix
# Override with: make CROSS_COMPILE=/path/to/arm-none-eabi-
# Or set ARM_TOOLCHAIN to the bin directory
CROSS_COMPILE ?= arm-none-eabi-
ifdef ARM_TOOLCHAIN
CROSS_COMPILE = $(ARM_TOOLCHAIN)/arm-none-eabi-
endif
PREFIX = $(CROSS_COMPILE)
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size

HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################
# CPU
CPU = -mcpu=cortex-m33

# FPU
FPU = -mfpu=fpv5-sp-d16
FLOAT-ABI = -mfloat-abi=hard

# MCU
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# AS defines
AS_DEFS =

# C defines
C_DEFS =  \
-DUSE_HAL_DRIVER \
-DSTM32H563xx \
-DETHERNET

# AS includes
AS_INCLUDES =

# C includes
C_INCLUDES =  \
-ICore/Inc \
-IDrivers/STM32H5xx_HAL_Driver/Inc \
-IDrivers/STM32H5xx_HAL_Driver/Inc/Legacy \
-IDrivers/CMSIS/Device/ST/STM32H5xx/Include \
-IDrivers/CMSIS/Core/Include \
-IMiddlewares/Third_Party/wolfSSL_wolfIP_wolfIP/wolfip \
-IMiddlewares/Third_Party/wolfSSL_wolfIP_wolfIP/wolfip/src \
-IwolfIP \
-IDrivers/CMSIS/Include \
-IMiddlewares/Third_Party/wolfSSL_wolfIP_wolfIP/wolfip/ \
-IMiddlewares/Third_Party/wolfSSL_wolfIP_wolfIP/wolfip/src/http

# Compile flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

#######################################
# LDFLAGS
#######################################
# Link script
LDSCRIPT = STM32H563xx_FLASH.ld

# Libraries
LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

#######################################
# Build the application
#######################################
# Default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

# List of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

# List of ASM objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir $@

#######################################
# Clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)

#######################################
# Flash the device
#######################################
flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $< 0x08000000

#######################################
# Dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***
