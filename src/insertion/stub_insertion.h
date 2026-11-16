
#include "stddef.h"

typedef struct 
{
	void *arr;
	size_t len;
} arr;

// TODO: check memory accesses to the elf size
arr insert_stub_elf64(arr elf, unsigned char *stub, size_t stub_size, size_t stub_entry_offset, int free_elf);
