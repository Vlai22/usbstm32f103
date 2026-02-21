# 1. Инструменты
COMPILER = arm-none-eabi-g++
CC       = arm-none-eabi-gcc
AS       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy

# 2. Настройки архитектуры и общие флаги
CPU_FLAGS = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
OPT_FLAGS = -Os -ffunction-sections -fdata-sections
DEFS      = -DSTM32F103xB

# 3. Пути
INCLUDES = -I./include \
           -ID:/Projects/Embedded/CMSIS/CMSIS_6/CMSIS/Core/Include \
           -ID:/Projects/Embedded/SCR_LIBS/stm32f1xx-hal-driver-fee494a92b5ad331f92ad21f76c66a5cb83773ee/Inc \
		   -ID:\Projects\Embedded\SCR_LIBS\cmsis-device-f1-master\Include

# Сборка флагов
COMMON_FLAGS = $(CPU_FLAGS) $(OPT_FLAGS) $(DEFS) $(INCLUDES) -g3
C++FLAGS     = $(COMMON_FLAGS) -std=c++20 -fno-exceptions -fno-rtti
CFLAGS       = $(COMMON_FLAGS)
# Важно: CPU_FLAGS добавлены в LDFLAGS
LDFLAGS      = $(CPU_FLAGS) -T STM32F103XB_FLASH.ld --specs=nosys.specs -Wl,--gc-sections

# 4. Исходники
SRCS_CPP = $(wildcard src/*.cpp)
SRCS_C   = $(wildcard src/*.c)
SRCS_S   = $(wildcard src/*.s)

OBJS = $(SRCS_C:.c=.o) $(SRCS_CPP:.cpp=.o) $(SRCS_S:.s=.o)

all: usb.bin

usb.bin: usb.elf
	$(OBJCOPY) -O binary $< $@

usb.elf: $(OBJS)
	$(COMPILER) $(OBJS) $(LDFLAGS) -o $@

%.o: %.cpp
	$(COMPILER) $(C++FLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(CPU_FLAGS) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

clean:
	rm -f src/*.o *.elf *.bin