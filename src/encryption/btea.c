
#include "btea.h"

void btea_encrypt(uint32_t *v, const uint32_t key[4], short n)
{
    uint32_t y, z, sum;
    unsigned p, rounds, e;

	rounds = 6 + 52/n;
	sum = 0;
	z = v[n-1];
	do {
	  sum += DELTA;
	  e = (sum >> 2) & 3;
	  for (p=0; p<n-1; p++) {
		y = v[p+1]; 
		z = v[p] += MX;
	  }
	  y = v[0];
	  z = v[n-1] += MX;
	} while (--rounds);
}

void btea_decrypt(uint32_t *v, const uint32_t key[4], short n)
{
	uint32_t y, z, sum;
    unsigned p, rounds, e;

	rounds = 6 + 52/n;
	sum = rounds*DELTA;
	y = v[0];
	do {
	  e = (sum >> 2) & 3;
	  for (p=n-1; p>0; p--) {
		z = v[p-1];
		y = v[p] -= MX;
	  }
	  z = v[n-1];
	  y = v[0] -= MX;
	  sum -= DELTA;
	} while (--rounds);
}

#include "errno.h"
#include "stdlib.h"
#include "stdio.h"
#include "time.h"
#include "random.h"
void print_big(void *big, size_t bytes, int fd)
{
	return;
	char str_byte[8];
	char byte;
	int err = 0;
	for (size_t i = 0; i < bytes && !err; i++)
	{
		byte = ((char *)big)[i];
		for (size_t bit_i = 0; bit_i < 8; bit_i++)
		{
			str_byte[7 - bit_i] = (byte & 1) + '0';
			byte = byte >> 1;
		}
		err = write(fd, str_byte, 8) < 0;
	}
	if (err) dprintf(2, "write err %i", errno);
}
int main()
{
	int file_fd = open("./tests/before_encryption", O_CREAT | O_TRUNC | O_WRONLY, 420);
	if (file_fd < 0) {dprintf(2, "Error: could not open before encryption filen\n"); return 0;}

	size_t n_bytes = 500 * 1000000; // Must be a multiple of 4
	printf("Tesing with %lu bytes.\n", n_bytes);
	char *random_bytes = (char*)malloc(n_bytes);
	if (!random_bytes) {dprintf(2, "Error: insufficient space on RAM\n"); return 1;}

	fill_randomly(random_bytes, n_bytes);
	print_big(random_bytes, n_bytes, file_fd);
	close(file_fd);

	__uint64_t key[2];
	fill_randomly(&key, 16);
	printf("Using key %lu %lu\n", key[0], key[1]);

	clock_t start = clock(), diff;

	btea_encrypt((uint32_t *)random_bytes, (uint32_t*)key, n_bytes);

	diff = clock() - start;
	double msec = diff * 1000 / (double)CLOCKS_PER_SEC;
	printf("Encryption: %lfms\n", msec);

	file_fd = open("./tests/after_encryption", O_CREAT | O_TRUNC | O_WRONLY, 420);
	if (file_fd < 0) {dprintf(2, "Error: could not open after encryption filen\n"); return 0;}

	print_big(random_bytes, n_bytes, file_fd);
	close(file_fd);

	start = clock();

	btea_decrypt((uint32_t *)random_bytes, (uint32_t*)key, n_bytes);

	diff = clock() - start;
	msec = diff * 1000 / (double)CLOCKS_PER_SEC;
	printf("Decryption: %lfms\n", msec);


	file_fd = open("./tests/after_decryption", O_CREAT | O_TRUNC | O_WRONLY, 420);
	if (file_fd < 0) {dprintf(2, "Error: could not open after decryption filen\n"); return 0;}

	print_big(random_bytes, n_bytes, file_fd);
	close(file_fd);
}
