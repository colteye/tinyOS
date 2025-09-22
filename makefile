TARGET = build/kernel

CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
LD = arm-none-eabi-gcc   # Use GCC as linker driver for convenience

CFLAGS  = -O0 -g -ffreestanding -nostdlib -Wall -Wextra -std=gnu99 \
          -mcpu=arm926ej-s -marm \
          -I. -Ios -Idrivers
ASFLAGS = -mcpu=arm926ej-s
LDFLAGS =  -g -T linker.ld

SRC_C = main.c drivers/uart.c os/scheduler.c # add other C files as needed
SRC_S = os/startup.S
OBJS_C  = $(patsubst %.c,build/%.o,$(SRC_C))
OBJS_S  = $(patsubst %.S,build/%.o,$(SRC_S))
OBJS = $(OBJS_S) $(OBJS_C)   # Ensure startup.o comes first

all: $(TARGET)

# Create build dir structure
$(shell mkdir -p build/drivers build/os)

# Compile C sources -> build/xxx.o
build/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble .S sources -> build/xxx.o
build/%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

# Link everything using gcc (not ld) to pull in symbols properly
$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -nostdlib -o $@ $(OBJS)

run: $(TARGET)
	qemu-system-arm -M versatilepb -m 32M \
	-cpu arm926 \
	-kernel $(TARGET) \
	-serial mon:vc \
	 -S -gdb tcp::1234

clean:
	rm -rf build $(TARGET)
