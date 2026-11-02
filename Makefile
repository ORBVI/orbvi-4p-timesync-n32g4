PREFIX ?= arm-none-eabi-
TIME_PROTOCOL ?= ZDA
ifeq ($(TIME_PROTOCOL),ZDA)
else ifeq ($(TIME_PROTOCOL),RMC)
else
$(error TIME_PROTOCOL must be ZDA or RMC)
endif
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE = $(PREFIX)size
OBJDUMP = $(PREFIX)objdump
BUILD_DIR = build/$(TIME_PROTOCOL)
TARGET = $(BUILD_DIR)/n32g4fr_timesync
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CPPFLAGS += -DTIME_PROTOCOL=TIME_PROTOCOL_$(TIME_PROTOCOL)
CFLAGS = $(MCU) -O2 -g3 -std=c11 -DN32G4FR -DUSE_STDPERIPH_DRIVER -ffunction-sections -fdata-sections -Wall -Wextra -Werror
INC = -ICore/Inc -Ifirmware/CMSIS/core -Ifirmware/CMSIS/device -Ifirmware/n32g4fr_std_periph_driver/inc
SOURCES = Core/Src/main.c Core/Src/time_protocol.c firmware/CMSIS/device/system_n32g4fr.c $(wildcard firmware/n32g4fr_std_periph_driver/src/*.c)
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES)) $(BUILD_DIR)/firmware/CMSIS/device/startup/startup_n32g4fr_gcc.o
LDFLAGS = $(MCU) -specs=nano.specs -specs=nosys.specs -T firmware/CMSIS/device/n32g4fr_flash.ld -Wl,--gc-sections,-Map=$(TARGET).map
HOST_TEST_FLAGS = -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -ICore/Inc
.PHONY: all clean test check-image
all: $(TARGET).elf $(TARGET).hex $(TARGET).bin check-image
$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
	$(SIZE) $@
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INC) -MMD -MP -c $< -o $@
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(MCU) -g3 -c $< -o $@
check-image: $(TARGET).elf $(TARGET).hex
	python3 tests/check_image.py $(TARGET).elf $(TARGET).hex
build/test_time_protocol_zda: tests/test_time_protocol.c Core/Src/time_protocol.c Core/Inc/time_protocol.h
	@mkdir -p build
	cc $(HOST_TEST_FLAGS) tests/test_time_protocol.c Core/Src/time_protocol.c -o $@
build/test_time_protocol_rmc: tests/test_time_protocol.c Core/Src/time_protocol.c Core/Inc/time_protocol.h
	@mkdir -p build
	cc $(HOST_TEST_FLAGS) -DTIME_PROTOCOL=TIME_PROTOCOL_RMC tests/test_time_protocol.c Core/Src/time_protocol.c -o $@
test: build/test_time_protocol_zda build/test_time_protocol_rmc
	./build/test_time_protocol_zda
	./build/test_time_protocol_rmc
	python3 -m unittest discover -s tests -p 'test_*.py'
clean:
	rm -rf build
-include $(OBJS:.o=.d)
