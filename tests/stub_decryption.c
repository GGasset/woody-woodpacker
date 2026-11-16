#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include "btea.h"

static void fail(const char *message)
{
	perror(message);
	exit(1);
}

static void deny_mprotect(unsigned int prot)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_mprotect, 0, 3),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, args[2])),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, prot, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog program = { .len = sizeof(filter) / sizeof(filter[0]), .filter = filter };
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) ||
		prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program))
		fail("seccomp");
}

static void deny_write(void)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_write, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog program = { .len = sizeof(filter) / sizeof(filter[0]), .filter = filter };
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) ||
		prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program))
		fail("seccomp");
}

static int pages_are_rx(unsigned char *data, size_t bytes, size_t page)
{
	uintptr_t start = (uintptr_t)data & ~(page - 1);
	uintptr_t end = ((uintptr_t)data + bytes + page - 1) & ~(page - 1);
	uintptr_t address = start, low, high;
	char line[512], perms[5];
	FILE *maps = fopen("/proc/self/maps", "r");

	if (!maps)
		fail("open maps");
	while (address < end && fgets(line, sizeof(line), maps)) {
		if (sscanf(line, "%lx-%lx %4s", &low, &high, perms) != 3 ||
			address < low || address >= high)
			continue;
		if (perms[0] != 'r' || perms[1] != '-' || perms[2] != 'x')
			break;
		address = high < end ? high : end;
	}
	fclose(maps);
	return address == end;
}

int main(int argc, char **argv)
{
	const uint32_t key[4] = {0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00};
	const unsigned char target[] = {0xb8, 0x78, 0x56, 0x34, 0x12, 0xc3};
	size_t page, bytes, data_pages, total, n, offsets[4];
	unsigned char *area, *stub, *data, *entry, *expected, *cipher;
	int64_t delta;
	uint64_t length;
	FILE *file;
	long stub_size;
	int positive, result, status;
	pid_t child;

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
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
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
	cipher = malloc(bytes);
	if (!cipher)
		fail("malloc");
	memcpy(cipher, data, bytes);
	if (!memcmp(data, expected, bytes)) {
		fprintf(stderr, "encryption did not change data\n");
		return 1;
	}
	memcpy(stub + offsets[0], key, sizeof(key));
	delta = data - stub;
	if (getenv("TEST_ADDRESS")) {
		uintptr_t address = strtoull(getenv("TEST_ADDRESS"), NULL, 0);
		uint64_t raw_delta = address - (uintptr_t)stub;
		memcpy(&delta, &raw_delta, sizeof(delta));
	}
	memcpy(stub + offsets[1], &delta, sizeof(delta));
	length = bytes;
	if (getenv("TEST_LENGTH"))
		length = strtoull(getenv("TEST_LENGTH"), NULL, 0);
	memcpy(stub + offsets[2], &length, sizeof(length));
	delta = entry - stub;
	memcpy(stub + offsets[3], &delta, sizeof(delta));
	if (mprotect(area, total, PROT_READ | PROT_EXEC))
		fail("mprotect");
	if (getenv("DENY_PROT")) {
		unsigned int prot = (unsigned int)strtoul(getenv("DENY_PROT"), NULL, 10);
		child = fork();
		if (child < 0)
			fail("fork");
		if (child == 0) {
			deny_mprotect(prot);
			((int (*)(void))stub)();
			_exit(99);
		}
		if (waitpid(child, &status, 0) < 0)
			fail("waitpid");
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 1 ||
			memcmp(data, prot == 7 ? cipher : expected, bytes)) {
			fprintf(stderr, "incorrect failure behavior for protection %u\n", prot);
			return 2;
		}
		return 1;
	}
	if (getenv("DENY_WRITE"))
		deny_write();
	result = ((int (*)(void))stub)();
	if (result != 0x12345678 || memcmp(data, expected, bytes) ||
		!pages_are_rx(data, bytes, page)) {
		fprintf(stderr, "wrong destination, data or permissions (n=%zu, positive=%d)\n", n, positive);
		return 1;
	}
	free(expected);
	free(cipher);
	if (munmap(area, total))
		fail("munmap");
	return 0;
}
