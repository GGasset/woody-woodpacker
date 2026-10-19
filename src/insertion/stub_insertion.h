
#include "stddef.h"

// Does not modify the stub. Modify the stub to call the previous entry before calling this as the previous entry point will be lost.
// TODO: check magic numbers and endianess
char *insert_stub_elf64(void *elf64, size_t elf64_len, unsigned char *stub, size_t stub_size, size_t stub_entry_offset);
