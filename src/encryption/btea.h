
#include "unistd.h"
#include "stdint.h"
#include "stddef.h"

#define DELTA 0x9e3779b9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))

void btea_encrypt(uint32_t *v, const uint32_t key[4], short n_bytes);
void btea_decrypt(uint32_t *v, const uint32_t key[4], short n_bytes);