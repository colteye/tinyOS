TARGET = build/kernel

CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
LD = arm-none-eabi-ld

CFLAGS  = -O0 -g -ffreestanding -nostdlib -Wall -Wextra -std=gnu99 \
          -mcpu=arm926ej-s -marm \
          -I. -Ios -Idrivers
ASFLAGS = -mcpu=arm926ej-s
LDFLAGS = -T linker.ld

SRC_C = main.c drivers/uart.c os/task.c
SRC_S = os/startup.S
OBJS  = $(patsubst %.c,build/%.o,$(SRC_C)) $(patsubst %.S,build/%.o,$(SRC_S))

all: $(TARGET)

# Create build dir structure
$(shell mkdir -p build/drivers build/os)

# Compile C sources -> build/xxx.o
build/%.o: %.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

# Assemble .S sources -> build/xxx.o
build/%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

# Link everything
$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

run: $(TARGET)
	qemu-system-arm -M versatilepb -m 128M \
	-cpu arm926 \
	-kernel $(TARGET) \
	-serial mon:vc

clean:
	rm -rf build $(TARGET)










