
#include "stddef.h"

typedef struct 
{
	void *arr;
	size_t len;
} array_return;

// TODO: check memory accesses to the elf size
array_return insert_stub_elf64(array_return elf, unsigned char *stub, size_t stub_size, size_t stub_entry_offset, int free_elf);
