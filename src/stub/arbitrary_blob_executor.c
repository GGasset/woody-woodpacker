
#include "sys/mman.h"
#include <bits/mman-linux.h>

void arbitrary_execution(void *raw_decrypt, size_t n_bytes, size_t offset)
{
	// Map memory that is writable and does not have an associated fd of n_bytes of size
	void *code_mapping = mmap(0, n_bytes, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (code_mapping == (void*)-1) return;

	// Memcpy
	for (size_t i = 0; i < n_bytes; i++) ((char *)code_mapping)[i] = ((char *)raw_decrypt)[i];

	// Set memory as read and execute
	if  (mprotect(code_mapping, n_bytes, PROT_READ | PROT_EXEC) >= 0)
		// Call function at memory offset
		((void (*)(void))((char*)code_mapping + offset))();

	// Unmap memory
	munmap(code_mapping, n_bytes);
}

/*int main()
{
	unsigned char src_stub_stub_bin[] = {
		0xb8, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8d, 0x35, 0x16, 0x00, 0x00, 0x00,
		0xbf, 0x01, 0x00, 0x00, 0x00, 0xba, 0x0e, 0x00, 0x00, 0x00, 0x0f, 0x05,
		0xb8, 0x3c, 0x00, 0x00, 0x00, 0x48, 0x31, 0xff, 0x0f, 0x05, 0x2e, 0x2e,
		0x2e, 0x2e, 0x57, 0x4f, 0x4f, 0x44, 0x59, 0x2e, 0x2e, 0x2e, 0x2e, 0x0a
	  };
	  unsigned int src_stub_stub_bin_len = 48;

	arbitrary_execution(src_stub_stub_bin, src_stub_stub_bin_len, 0);
}*/