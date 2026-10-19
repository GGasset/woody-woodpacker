
#include "unistd.h"
#include "stdlib.h"
#include "fcntl.h"
#include "elf.h"

#include "stub_insertion.h"

typedef struct 
{
	void *arr;
	size_t len;
} array_return;

static size_t	min(size_t a, size_t b)
{
	return (a * (a <= b) + b * (b < a));
}

void	*ft_realloc(void *old, size_t old_len, size_t new_len, int free_old)
{
	char	*out;

	out = malloc(new_len);
	for (size_t i = 0; i < new_len; i++) out[i] = 0;

	size_t i = 0;
	while (old && out && i < min(old_len, new_len))
	{
		out[i] = ((char *)old)[i];
		i++;
	}
	if (free_old)
		free(old);
	return (out);
}

char *insert_padding(char *s, size_t s_len, size_t offset, size_t n_zeroed_bytes, int free_s)
{
	if (!s) return 0;
	
	if (offset > s_len) { if (free_s) free(s); return 0;}

	size_t out_len = s_len + n_zeroed_bytes + 1;
	char *out = malloc(out_len);
	out[out_len] = 0;

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

	out = ft_realloc(0, 0, out_len, false);
	for (size_t i = 0; i < insert_offset; i++)
		out[i] += s[i];

	for (size_t i = 0; i < to_insert_len; i++)
		out[insert_offset + i] = to_insert[i];
	
	for (size_t i = insert_offset; i < s_len; i++)
		out[insert_offset + to_insert_len + i] = s[i];
	
ret:
	if (free_s) free(s);
	if (free_to_insert) free(to_insert);

	return out;
}

array_return read_file(char *path)
{
	array_return out;
	out.arr = 0;
	out.len = 0;

	if (!path) return out;
	int fd = open(path, O_RDONLY);
	if (fd < 0) return out;

	
	char tmp;
	while (read(fd, &tmp, 1) > 0)
	{
		out.arr = ft_realloc(out.arr, out.len, out.len + 1, true);
		if (!out.arr) return out;
		((char *)out.arr)[out.len] = tmp;
		out.len++;
	}

	close(fd);
	return out;
}

char *insert_stub_elf64(void *elf64, size_t elf64_len, unsigned char *stub, size_t stub_size, size_t stub_entry_offset)
{
	if (!elf64 || !stub || stub_size) return 0;

	Elf64_Ehdr *hdr = (Elf64_Ehdr*)elf64;
	if (hdr->e_ident[EI_CLASS] != 2) return 0;

	// Insert new program header
	size_t phdr_insert_off = hdr->e_phoff + hdr->e_phnum * hdr->e_phentsize;

	Elf64_Phdr inserted_hdr;

	// On modern, typical, computers almost all alignment is of 4096bytes
	size_t padding_size = 0x1000 - sizeof(Elf64_Phdr);
	
	char *out = 0;
	out = insert_padding(elf64, elf64_len, phdr_insert_off, padding_size, false);
	elf64_len += padding_size;

	out = mem_insert(out, elf64_len, &inserted_hdr, sizeof(inserted_hdr), phdr_insert_off, true, false);
	elf64_len += sizeof(inserted_hdr);

	hdr = out;
	hdr->e_phnum++;
	Elf64_Phdr *p_hdrs = (Elf64_Phdr *)(out + hdr->e_phoff);

	long max_vaddr_end = 0;
	// Modify program/section headers offset after header insertion
	// Skip the added header
	for (size_t i = 0; i < hdr->e_phnum - 1; i++)
	{
		p_hdrs[i].p_offset += 0x1000;

		if (p_hdrs[i].p_type != PT_LOAD) continue;

		// Get max virtual address
		size_t vaddr_end = p_hdrs[i].p_vaddr + p_hdrs[i].p_memsz;
		max_vaddr_end += (vaddr_end - max_vaddr_end) * (vaddr_end > max_vaddr_end);
	}
	for (size_t i = 0; i < hdr->e_shnum; i++)
		(((Elf64_Shdr*)(out)) + hdr->e_shoff + i)->sh_offset += 0x1000;


	// Get next aligned vaddr
	size_t new_vaddr = max_vaddr_end + (0x1000 - max_vaddr_end % 0x1000) % 0x1000;

	// Get padding needed to insert into the next aligned file position
	size_t padding_for_stub_alignment = 0x1000 - (elf64_len % 0x1000) % 0x1000;

	Elf64_Phdr *inserted_header = &p_hdrs[hdr->e_phnum];
	inserted_header->p_type = PT_LOAD;
	inserted_header->p_flags = PF_R | PF_X;
	inserted_header->p_offset = elf64_len + padding_for_stub_alignment;
	inserted_header->p_vaddr = new_vaddr;
	inserted_header->p_paddr = 0;
	inserted_header->p_filesz = stub_size;
	inserted_header->p_memsz = stub_size;
	inserted_header->p_align = 0x1000;

	out = ft_realloc(out, elf64_len, elf64_len + padding_for_stub_alignment, true);
	elf64_len += padding_for_stub_alignment;

	out = ft_realloc(out, elf64_len, elf64_len + stub_size, true);
	for (size_t i = 0; i < stub_size; i++)
		out[elf64_len + i] = stub[i];
	elf64_len += stub_size;

	hdr = out;
	hdr->e_entry = new_vaddr + stub_entry_offset;

	return out;
}
