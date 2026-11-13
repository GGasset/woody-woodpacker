#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "btea.h"

static void fail(const char *message)
{
	perror(message);
	exit(1);
}

int main(int argc, char **argv)
{
	const uint32_t key[4] = {0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00};
	const unsigned char target[] = {0xb8, 0x78, 0x56, 0x34, 0x12, 0xc3};
	size_t page, bytes, data_pages, total, n, offsets[4];
	unsigned char *area, *stub, *data, *entry, *expected;
	int64_t delta;
	uint64_t length;
	FILE *file;
	long stub_size;
	int positive, result;

	if (argc != 8)
		return 2;
	n = strtoul(argv[2], NULL, 10);
	positive = atoi(argv[3]);
	for (int i = 0; i < 4; i++)
		offsets[i] = strtoul(argv[4 + i], NULL, 10);
	page = (size_t)sysconf(_SC_PAGESIZE);
	if (page != 4096 || n < 2 || n > 32767)
		return 2;
	bytes = n * sizeof(uint32_t);
	/* Start near a page boundary, exercising unaligned and multi-page ranges. */
	data_pages = (page - 4 + bytes + page - 1) / page;
	total = (2 * data_pages + 4) * page;
	area = mmap(NULL, total, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (area == MAP_FAILED)
		fail("mmap");
	stub = area + (data_pages + 2) * page;
	data = positive ? stub + 2 * page + page - 4 : area + 2 * page - 4;
	entry = positive ? stub + page : area;
	memcpy(entry, target, sizeof(target));
	file = fopen(argv[1], "rb");
	if (!file || fseek(file, 0, SEEK_END) || (stub_size = ftell(file)) <= 0 ||
		(size_t)stub_size > page || fseek(file, 0, SEEK_SET) ||
		fread(stub, 1, stub_size, file) != (size_t)stub_size)
		fail("read stub");
	fclose(file);
	if (offsets[0] + 16 > (size_t)stub_size || offsets[1] + 8 > (size_t)stub_size ||
		offsets[2] + 8 > (size_t)stub_size || offsets[3] + 8 > (size_t)stub_size)
		return 2;
	expected = malloc(bytes);
	if (!expected)
		fail("malloc");
	for (size_t i = 0; i < n; i++) {
		uint32_t word = (uint32_t)i * 2654435761U ^ 0xdeadbeefU;
		memcpy(expected + i * 4, &word, 4);
	}
	memcpy(data, expected, bytes);
	btea_encrypt((uint32_t *)data, key, (short)n);
	if (!memcmp(data, expected, bytes)) {
		fprintf(stderr, "encryption did not change data\n");
		return 1;
	}
	memcpy(stub + offsets[0], key, sizeof(key));
	delta = data - stub;
	memcpy(stub + offsets[1], &delta, sizeof(delta));
	length = bytes;
	memcpy(stub + offsets[2], &length, sizeof(length));
	delta = entry - stub;
	memcpy(stub + offsets[3], &delta, sizeof(delta));
	if (mprotect(area, total, PROT_READ | PROT_EXEC))
		fail("mprotect");
	result = ((int (*)(void))stub)();
	if (result != 0x12345678 || memcmp(data, expected, bytes)) {
		fprintf(stderr, "wrong destination or decrypted data (n=%zu, positive=%d)\n", n, positive);
		return 1;
	}
	free(expected);
	if (munmap(area, total))
		fail("munmap");
	return 0;
}
