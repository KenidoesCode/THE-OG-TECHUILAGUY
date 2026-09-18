CXX := g++
LD := ld
AS := as
OBJCOPY := objcopy

CXXFLAGS := -m32 -ffreestanding -fno-exceptions -fno-rtti \
            -fno-stack-protector -nostdlib -nodefaultlibs \
            -nostartfiles -Wall -Wextra -O2

ASFLAGS := --32

LDFLAGS := -m elf_i386 -T kernel/linker.ld -z noexecstack

SOURCES := \
    boot/boot.S \
    kernel/kernel.cpp \
    kernel/serial.cpp \
    memory/memory.cpp \
    heap/heap.cpp \
    elf/elf.cpp \
    gdt/gdt.cpp \
    gdt/gdt_flush.S \
    paging/paging.cpp \
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

# Userland programs: real flat machine-code images, assembled and
# linked with the freestanding toolchain like everything else, then
# extracted to raw binaries and re-embedded as ordinary object files
# (via `objcopy -I binary`, which synthesizes _binary_<path>_start/_end
# symbols) so the kernel image can copy one into a page at scheduler
# run time — see scheduler_create_user_task. Never loaded by an ELF
# loader; user.ld exists only to give objcopy a flat layout to extract.
USERLAND_PROGRAMS := hello evil kernel_peek neighbor_peek
USERLAND_BINS := $(patsubst %,userland/%.bin,$(USERLAND_PROGRAMS))
USERLAND_EMBED_OBJECTS := $(patsubst %,userland/%_embed.o,$(USERLAND_PROGRAMS))

# Real ELF32/i386 ET_EXEC userland programs, consumed by the kernel's
# own ELF loader (elf/elf.hpp, scheduler_create_elf_user_task) rather
# than copied as a flat binary — see docs/ADR/0004-elf-loader.md. Each
# one is embedded via objcopy -I binary directly on its *linked ELF
# file* (never flattened first, unlike USERLAND_PROGRAMS above), so
# the bytes the kernel parses at boot are a genuine ELF image with a
# real header and program header table.
ELF_USERLAND_PROGRAMS := elf_hello elf_write_to_code
ELF_USERLAND_ELFS := $(patsubst %,userland/%.elf,$(ELF_USERLAND_PROGRAMS))
ELF_USERLAND_EMBED_OBJECTS := $(patsubst %,userland/%_elfembed.o,$(ELF_USERLAND_PROGRAMS))

all: techuilaguy-os

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

userland/%.elf: userland/%.S userland/user.ld
	$(AS) $(ASFLAGS) $< -o $(@:.elf=.o)
	$(LD) -m elf_i386 -T userland/user.ld $(@:.elf=.o) -o $@

userland/%.bin: userland/%.elf
	$(OBJCOPY) -O binary $< $@

userland/%_embed.o: userland/%.bin
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 $< $@

userland/elf_hello.elf: userland/elf_hello.S userland/elf_user.ld
	$(AS) $(ASFLAGS) userland/elf_hello.S -o userland/elf_hello.o
	$(LD) -m elf_i386 -T userland/elf_user.ld userland/elf_hello.o -o $@

userland/elf_write_to_code.elf: userland/elf_write_to_code.S userland/elf_write_to_code.ld
	$(AS) $(ASFLAGS) userland/elf_write_to_code.S -o userland/elf_write_to_code.o
	$(LD) -m elf_i386 -T userland/elf_write_to_code.ld userland/elf_write_to_code.o -o $@

userland/%_elfembed.o: userland/%.elf
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 $< $@

techuilaguy-os: $(OBJECTS) $(USERLAND_EMBED_OBJECTS) $(ELF_USERLAND_EMBED_OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) $(USERLAND_EMBED_OBJECTS) $(ELF_USERLAND_EMBED_OBJECTS) -o $@

clean:
	rm -f $(OBJECTS) techuilaguy-os techuilaguy-os.iso
	rm -f userland/*.o userland/*.elf userland/*.bin
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
