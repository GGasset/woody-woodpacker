
#include "unistd.h"
#include "stdlib.h"
#include "fcntl.h"
#include "elf.h"
#include "stdio.h"

#include "stub_insertion.h"

#ifndef true
#define true 1
#define false 0
#endif

static size_t	min(size_t a, size_t b)
{
	return (a * (a <= b) + b * (b < a));
}

size_t ft_strlen(char *s)
{
	size_t i = 0;
	for (; s && s[i]; i++);
	return i;
}

void	*ft_realloc(void *old, size_t old_len, size_t new_len, int free_old)
{
	char	*out;

	out = malloc(new_len);
	if (!out) goto ret;
	for (size_t i = 0; i < new_len; i++) out[i] = 0;

	size_t i = 0;
	while (old && out && i < min(old_len, new_len))
	{
		out[i] = ((char *)old)[i];
		i++;
	}

ret:
	if (free_old) free(old);
	return (out);
}

char *insert_padding(char *s, size_t s_len, size_t offset, size_t n_zeroed_bytes, int free_s)
{
	if (!s) return 0;
	
	if (offset > s_len) { if (free_s) free(s); return 0;}

	size_t out_len = s_len + n_zeroed_bytes + 1;
	char *out = malloc(out_len);
	if (!out) { if (free_s) free(s); return 0;}

	for (size_t i = 0; i < offset; i++) out[i] = s[i];
	for (size_t i = 0; i < n_zeroed_bytes; i++) out[offset + i] = 0;
	for (size_t i = offset; i < s_len; i++) out[n_zeroed_bytes + i] = s[i];

	if (free_s) free(s);
	return out;
}

char *mem_insert(char *s, size_t s_len, char *to_insert, size_t to_insert_len, size_t insert_offset, int free_s, int free_to_insert)
{
	if (!s || !to_insert) return 0;
	
	char *out = 0;
	size_t out_len = s_len + to_insert_len;
	if (insert_offset > s_len) goto ret;

	out = ft_realloc(0, 0, out_len, 0);
	if (!out) goto ret;
	for (size_t i = 0; i < insert_offset; i++)
		out[i] = s[i];

	for (size_t i = 0; i < to_insert_len; i++)
		out[insert_offset + i] = to_insert[i];
	
	for (size_t i = 0; i < s_len - insert_offset; i++)
		out[insert_offset + to_insert_len + i] = s[insert_offset + i];
	
ret:
	if (free_s) free(s);
	if (free_to_insert) free(to_insert);

	return out;
}

char *mem_append_0s(char *s, size_t s_len, size_t n_zeros, int free_s)
{
	return insert_padding(s, s_len, s_len, n_zeros, free_s);
}

char *mem_append(char *s, size_t s_len, char *to_append, size_t to_append_len, int free_s, int free_to_append)
{
	return mem_insert(s, s_len, to_append, to_append_len, s_len, free_s, free_to_append);
}

// Inserts padding before and after the to_append blob to append the blob aligned and leave the string aligned
arr append_aligned_mem(arr s, char *to_append, size_t to_append_len, int free_s, int free_to_append, size_t alignment, size_t *out_content_offset)
{
	char *tmp = 0;

	if (!s.arr || !to_append)
	{
		goto fail;
	}

	size_t EOF_padding = (alignment - (s.len % alignment)) % alignment;

	tmp = mem_append_0s(s.arr, s.len, EOF_padding, free_s);
	s.arr = 0;
	size_t tmp_len = s.len + EOF_padding;
	if (!tmp) goto fail;

	tmp = mem_append(tmp, tmp_len, to_append, to_append_len, true, free_to_append);
	if (out_content_offset && tmp) *out_content_offset = tmp_len;
	to_append = 0;
	tmp_len += to_append_len;
	if (!tmp) goto fail;

	EOF_padding = (alignment - (s.len % alignment)) % alignment;
	arr out;
	out.arr = mem_append_0s(tmp, tmp_len, EOF_padding, true);
	tmp = 0;
	out.len = 0;

	if (!out.arr) goto fail;
	out.len = tmp_len + EOF_padding;

	return out;

fail:
	// Do the free thing if specified by the user
	free(tmp);
	if (free_s) free(s.arr);
	if (free_to_append) free(to_append);
	return out;
}

arr read_file(char *path)
{
	// Read 4KB per call
	const size_t read_size = 1024 * 4;

	arr out;
	out.arr = 0;
	out.len = 0;

	if (!path) return out;
	int fd = open(path, O_RDONLY);
	if (fd < 0) return out;

	size_t actual_read = 0;
	char buff[read_size];
	for (size_t read_n = 0; (actual_read = read(fd, buff, read_size)) > 0; read_n++)
	{
		out.arr = ft_realloc(out.arr, out.len, out.len + actual_read, 1);
		if (!out.arr) return out;
		out.len += actual_read;

		size_t read_start = read_size * read_n;
		for (size_t i = 0; i < actual_read; i++) ((char *)out.arr)[read_start + i] = buff[i];

	}

	close(fd);
	return out;
}

// Creates a new program header table at the end
arr append_stub_elf64(arr elf, unsigned char *stub, size_t stub_size, size_t stub_entry_offset, int free_elf)
{
	arr ret;
	ret.arr = elf.arr;
	ret.len = elf.len;

	char *elf64 = elf.arr;

	if (!elf64 || !stub || !stub_size) {free(elf.arr); ret.len = 1; return ret;}

	Elf64_Ehdr *hdr = (Elf64_Ehdr*)elf64;
	if ((hdr->e_ident[0] == "\177ELF"[0] && hdr->e_ident[1] == "\177ELF"[1] && hdr->e_ident[2] == "\177ELF"[2] && hdr->e_ident[3] == "\177ELF"[3]) == 0) {free(elf.arr); ret.len = 4; return ret;}
	if (hdr->e_ident[EI_CLASS] != 2 || hdr->e_type != ET_EXEC) {free(elf.arr); ret.len = 2; return ret;}

	
	size_t prev_n_phdrs = hdr->e_phnum;
	size_t n_phdrs = prev_n_phdrs + 1;



	Elf64_Phdr *old_phdr_table = (Elf64_Phdr*)((char*)elf.arr + hdr->e_phoff);
	Elf64_Phdr new_phdr_table[n_phdrs];
	size_t table_size = sizeof(Elf64_Phdr) * n_phdrs;

	for (size_t i = 0; i < prev_n_phdrs; i++)
		new_phdr_table[i] = old_phdr_table[i];

	char *to_append = mem_append((char*)new_phdr_table, table_size, (char*)stub, stub_size, false, false);
	size_t to_append_len = table_size + stub_size;

	// On modern, typical, computers almost all alignment is of 4096bytes
	size_t alignment = 0x1000;

	size_t phoff = 0;
	elf = append_aligned_mem(elf, to_append, to_append_len, free_elf, true, alignment, &phoff);
	if (!elf.arr) return elf;
	elf64 = elf.arr;
	hdr = (Elf64_Ehdr*)elf64;

	long max_vaddr_end = 0;
	// Modify program/section headers offset after header insertion
	// Skip the added header
	for (size_t i = 0; i < hdr->e_phnum; i++)
	{
		// Get max virtual address
		size_t vaddr_end = new_phdr_table[i].p_vaddr + new_phdr_table[i].p_memsz;
		max_vaddr_end += (vaddr_end - max_vaddr_end) * (vaddr_end > max_vaddr_end);
	}

	// Get next aligned vaddr
	size_t new_vaddr = max_vaddr_end + (alignment - max_vaddr_end % alignment) % alignment;

	hdr->e_phoff = phoff;
	hdr->e_phnum = n_phdrs;
	hdr->e_entry = new_vaddr + table_size + stub_entry_offset;

	size_t aligned_to_append_size = to_append_len + (alignment - (to_append_len % alignment)) % alignment;

	Elf64_Phdr new_phdr;
	new_phdr.p_align = alignment;
	new_phdr.p_offset = phoff;
	new_phdr.p_filesz = to_append_len;
	new_phdr.p_memsz = to_append_len;
	new_phdr.p_type = PT_LOAD;
	new_phdr.p_flags = PF_R | PF_X;
	new_phdr.p_vaddr = new_vaddr;
	new_phdr.p_paddr = new_vaddr;

	Elf64_Phdr *phdr_table = ((Elf64_Phdr*)((char*)elf.arr + phoff));
	Elf64_Phdr *appended_phdr = phdr_table + prev_n_phdrs;
	*appended_phdr = new_phdr;

	for (size_t i = 0; i < hdr->e_phnum; i++)
	{
		char *section_ptr = (char*)elf.arr + phdr_table[i].p_offset;

		switch (phdr_table[i].p_type)
		{
		case PT_PHDR:
		{
			Elf64_Phdr *phdr = phdr_table + i;
			phdr->p_align = alignment;
			phdr->p_offset = phoff;
			phdr->p_filesz = table_size;
			phdr->p_memsz = table_size;
			phdr->p_type = PT_PHDR;
			phdr->p_flags = PF_R;
			phdr->p_vaddr = new_vaddr;
			phdr->p_paddr = new_vaddr;

			printf("%zx\n", phoff);
			break;
		}
		
		default:
			break;
		}
	}

	return elf;
}

#include "stdio.h"

int main(int argc, char **argv)
{
	unsigned char src_stub_bin[] = {
		0xb8, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8d, 0x35, 0x16, 0x00, 0x00, 0x00,
		0xbf, 0x01, 0x00, 0x00, 0x00, 0xba, 0x0e, 0x00, 0x00, 0x00, 0x0f, 0x05,
		0xb8, 0x3c, 0x00, 0x00, 0x00, 0x48, 0x31, 0xff, 0x0f, 0x05, 0x2e, 0x2e,
		0x2e, 0x2e, 0x57, 0x4f, 0x4f, 0x44, 0x59, 0x2e, 0x2e, 0x2e, 0x2e, 0x0a
	};
	unsigned int src_stub_bin_len = 48;

	if (argc < 2) { printf("Usage: ./woody-woodpacker {infile} [outfile]\n"); return 0;}

	arr elf = read_file(argv[1]);

	if (!elf.arr || !elf.len) {printf("Error while reading file.\n"); return 0;}

	//elf = insert_stub_elf64(elf, src_stub_stub_bin, src_stub_stub_bin_len, 0, 1);
	elf = append_stub_elf64(elf, src_stub_bin, src_stub_bin_len, 0, true);
	if (!elf.arr)
	{
		printf("Error while processing and injecting to file. ");
		switch (elf.len)
		{
		case 1:
			printf("Input was null\n");
			break;
		case 2:
			printf("Not an executable 64 bit elf\n");
			break;
		case 3:
			printf("Malloc error\n");
			break;
		case 4:
			printf("Not an Elf file\n");
			break;
		case 0:
		default:
			printf("\n");
			break;
		}
		return 0;
	}
	
	char *outfile = 0;
	if (argc < 3)
	{
		char extension[] = ".hacked";

		size_t old_size = ft_strlen(argv[1]);
		size_t extension_size = ft_strlen(extension);
		size_t new_size = old_size + extension_size + 1;

		outfile = ft_realloc(argv[1], old_size, new_size, 0);

		for (size_t i = 0; outfile && i < extension_size; i++) outfile[old_size + i] = extension[i];


		//for (size_t i = old_size; outfile && i < new_size; i++) outfile[i] = extension[i - old_size];
	}
	else outfile = ft_realloc(argv[3], ft_strlen(argv[3]) + 1, ft_strlen(argv[3]) + 1, 0);
	if (!outfile) {printf("Memory error while creating out file path\n"); free(elf.arr); return 0;}

	// Open file with correct permissions
	int outfd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	if (outfd < 0) {printf("Error opening \"%s\"\n", outfile); free(outfile); free(elf.arr); return 0;}

	printf("Writing to \"%s\"\n", outfile);
	if (write(outfd, elf.arr, elf.len) < elf.len) {printf("Error writing to file.\n");}

	close(outfd);
	free(outfile);
	free(elf.arr);

	return 0;
}
