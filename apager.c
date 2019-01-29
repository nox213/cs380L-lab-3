#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <elf.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "apager.h"

int main(int argc, char *argv[])
{
	Elf64_Ehdr elf_header;
	Elf64_Phdr phdr;
	int fd, poff;

	if (argc < 2) {
		fprintf(stderr, "./apager exe_file\n");
		exit(EXIT_FAILURE);
	}

	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "Fail to open loaded file\n");
		goto out;
	}

	if (read(fd, &elf_header, sizeof(Elf64_Ehdr)) < 0) {
		fprintf(stderr, "read error %d\n", __LINE__);
		goto out;
	}

	if (memcmp(&elf_header, ELFMAG, SELFMAG) != 0) {
		fprintf(stderr, "File format error\n");
		goto out;
	}

	show_elf_header(&elf_header);

	poff = elf_header.e_phoff;
	lseek(fd, poff, SEEK_SET);
	read(fd, &phdr, sizeof(Elf64_Phdr));
	printf("%p\n", mmap(phdr.p_vaddr + 0x1000, phdr.p_filesz, 
				PROT_READ, MAP_PRIVATE | MAP_FIXED,
				fd, phdr.p_offset));

out:
	close(fd);
	exit (EXIT_SUCCESS);
}

void show_elf_header(Elf64_Ehdr *ep)
{
	printf("e_ident: %s\n", ep->e_ident);
	printf("e_entry: %p\n", ep->e_entry);
	printf("e_phoff: %d\n", ep->e_phoff);
	printf("e_shoff: %d\n", ep->e_shoff);
	printf("sizeof Elf64_Ehdr: %u\n", sizeof(Elf64_Ehdr));
	printf("e_ehsize: %u\n", ep->e_ehsize);
	printf("e_phentsize: %u\n", ep->e_phentsize);
	printf("e_phnum: %u\n", ep->e_phnum);
}

int load_elf_binary(int fd, Elf64_Ehdr *ep)
{
	Elf64_Phdr phdr;
	Elf64_Addr elf_entry;
	int i;

	memset(&phdr, 0, sizeof(Elf64_Phdr));
	lseek(fd, ep->e_phoff, SEEK_SET);
	for (i = 0; i < ep->e_phnum; i++) {
		int elf_prot = 0, elf_flags;
		Elf64_Addr vaddr;

		if (read(fd, &phdr, sizeof(Elf64_Phdr)) < 0) {
			fprintf(stderr, "read erorr on phdr\n");
			return -1;
		}
		
		if (phdr.p_type != PT_LOAD)
			continue;

		if (phdr.p_flags & PF_R)
			elf_prot |= PROT_READ;
		if (phdr.p_flags & PF_W)
			elf_prot |= PROT_WRITE;
		if (phdr.p_flags & PF_X)
			elf_prot |= PROT_EXEC;
		
		elf_flags = MAP_PRIVATE | MAP_FIXED | MAP_EXECUTABLE;

		vaddr = phdr.p_vaddr;

		if (elf_map(vaddr, 0, elf_prot, elf_flags, fd, &phdr) < 0) {
			fprintf(stderr, "elf_map error\n");
			return -1;
		}
	}

	return 0;
}

int elf_map(Elf64_Addr addr, unsigned long total_size, int prot, int type, 
		int fd, Elf64_Phdr *pp)
{
	Elf64_Addr map_addr;
	unsigned long size = pp->p_filesz + ELF_PAGEOFFSET(pp->p_vaddr);
	unsigned long off = pp->p_offset - ELF_PAGEOFFSET(pp->p_vaddr);
	addr = ELF_PAGESTART(addr);
	size = ELF_PAGEALIGN(size);

	if (!size)
		return addr;
	
	return mmap(addr, size, prot, type, fd, off);
}

