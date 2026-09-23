CC = cc
CFLAGS = -Wall -Wextra -Werror
NASM = nasm
LD = ld
OBJCOPY = objcopy

.DEFAULT_GOAL := all

STUB_ASM = src/stub/stub.asm
STUB_OBJ = src/stub/stub.o
STUB_LINKED = src/stub/stub_linked
STUB_BIN = src/stub/stub.bin

# The linked ELF only resolves internal relocations. It is not a runnable test
# binary because the final stub needs its placeholders patched by the packer.
$(STUB_OBJ): $(STUB_ASM)
	$(NASM) -f elf64 $< -o $@

$(STUB_LINKED): $(STUB_OBJ)
	$(LD) $< -o $@

$(STUB_BIN): $(STUB_LINKED)
	$(OBJCOPY) -O binary -j .text $< $@

all: $(STUB_BIN)

clean:
	rm -f $(STUB_OBJ) $(STUB_LINKED)

fclean: clean
	rm -f $(STUB_BIN) woody_woodpacker

re: fclean all

.PHONY: all clean fclean re
