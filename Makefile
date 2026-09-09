cc = cc
NASM = nasm
CFLAGS = -Wall -Wextra -Werror

STUB_ASM = src/stub/stub.asm 
STUB_OBJ = src/stub/stub.o 
STUB_ELF = src/stub/stub_test
STUB_BIN = src/stub/stub.bin 

# Regla crítica: stub.bim se genera siempre antes que woody_woodpacker
$(STUB_BIN): $(STUB_ASM)
	$(NASM) -f elf64 $< -o $(STUB_OBJ) 	# ensambla a objeto ELF64
	ld $(STUB_OBJ) -o $(STUB_ELF)		# enlaza para probar standalone
	objcopy -O binary -j .text $(STUB_ELF) $@ # extraer SOLO los bytes de .text
	xxd -i $(STUB_BIN)

# Embeber con xxd (más robusto para entrega)
#src/stub/stub.h: $(STUB_BIN)
#	xxd -i $< > $@

#woody_woodpacker: src/main.c src/encryption/btea.c src/encryption/random.c src/stub/stub.h
#	$(CC) $(CFLAGS) $^ -o $@

all: $(STUB_BIN)

clean:
	rm -f $(STUB_OBJ) $(STUB_ELF) $(STUB_BIN)

fclean: clean
#	rm -f woody_woodpacker

re: fclean all
.PHONY: all clean fclean re
