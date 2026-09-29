PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE = $(PREFIX)size
OBJDUMP = $(PREFIX)objdump
TARGET = build/n32g4fr_timesync
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS = $(MCU) -O2 -g3 -std=c11 -DN32G4FR -DUSE_STDPERIPH_DRIVER -ffunction-sections -fdata-sections -Wall -Wextra -Werror
INC = -ICore/Inc -Ifirmware/CMSIS/core -Ifirmware/CMSIS/device -Ifirmware/n32g4fr_std_periph_driver/inc
SOURCES = Core/Src/main.c Core/Src/time_protocol.c firmware/CMSIS/device/system_n32g4fr.c $(wildcard firmware/n32g4fr_std_periph_driver/src/*.c)
OBJS = $(patsubst %.c,build/%.o,$(SOURCES)) build/firmware/CMSIS/device/startup/startup_n32g4fr_gcc.o
LDFLAGS = $(MCU) -specs=nano.specs -specs=nosys.specs -T firmware/CMSIS/device/n32g4fr_flash.ld -Wl,--gc-sections,-Map=$(TARGET).map
.PHONY: all clean test check-image
all: $(TARGET).elf $(TARGET).hex $(TARGET).bin check-image
$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
	$(SIZE) $@
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -MMD -MP -c $< -o $@
build/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(MCU) -g3 -c $< -o $@
check-image: $(TARGET).elf $(TARGET).hex
	python3 tests/check_image.py $(TARGET).elf $(TARGET).hex
test:
	@mkdir -p build
	cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -ICore/Inc tests/test_time_protocol.c Core/Src/time_protocol.c -o build/test_time_protocol
	./build/test_time_protocol
	python3 -m unittest discover -s tests -p test_board_link.py
clean:
	rm -rf build
-include $(OBJS:.o=.d)
