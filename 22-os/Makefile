CXX := g++
LD := ld
AS := as

CXXFLAGS := -m32 -ffreestanding -fno-exceptions -fno-rtti \
            -fno-stack-protector -nostdlib -nodefaultlibs \
            -nostartfiles -Wall -Wextra -O2

ASFLAGS := --32

LDFLAGS := -m elf_i386 -T kernel/linker.ld

SOURCES := \
    boot/boot.S \
    kernel/kernel.cpp \
    kernel/serial.cpp \
    memory/memory.cpp \
    interrupts/interrupts.cpp \
    interrupts/pic.cpp \
    interrupts/isr.S \
    scheduler/scheduler.cpp \
    scheduler/pit.cpp \
    drivers/keyboard.cpp \
    drivers/keyboard_translation.cpp \
    syscalls/syscalls.cpp \
    vfs/vfs.cpp \
    security/security.cpp

OBJECTS := $(SOURCES:.cpp=.o)
OBJECTS := $(OBJECTS:.S=.o)

all: techuilaguy-os

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

techuilaguy-os: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	rm -f $(OBJECTS) techuilaguy-os techuilaguy-os.iso
	rm -rf iso

iso: techuilaguy-os
	mkdir -p iso/boot/grub
	cp techuilaguy-os iso/boot/techuilaguy-os
	printf '%s\n' \
		'set timeout=0' \
		'set default=0' \
		'menuentry "Techuilaguy OS" {' \
		'    multiboot /boot/techuilaguy-os' \
		'    boot' \
		'}' > iso/boot/grub/grub.cfg
	grub-mkrescue -o techuilaguy-os.iso iso

run: iso
	qemu-system-i386 -cdrom techuilaguy-os.iso -serial stdio
