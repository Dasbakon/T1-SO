/* Author(s): <Your name(s) here>
 * Creates operating system image suitable for placement on a boot disk
*/
/* TODO: Comment on the status of your submission. Largely unimplemented */
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512				/* floppy sector size in bytes */
#define BOOTLOADER_SIG_OFFSET 0x1fe /* offset for boot loader signature */
// more defines...

/* Reads in an executable file in ELF format*/
Elf32_Phdr *read_exec_file(FILE **execfile, char *filename, Elf32_Ehdr **ehdr)
{
	Elf32_Word e_phoff = 0x0;
	Elf32_Half e_phentsize = 0x0;
	Elf32_Half e_phnum = 0x0;
	Elf32_Phdr *pheader;
	int phtable_size = 0;
	char *buffer;

	*execfile = fopen(filename, "r");
	assert(execfile);

	buffer = (char *)malloc(SECTOR_SIZE * sizeof(char));
	assert(buffer && "Nao foi possivel alocar o buffer\n");

	// Agora, com o _execfile_ aberto, temos que:
	// 1 - ler o primeiro bloco e armazenar em um _buffer_
	// 2 - Serializar o _buffer_ e passar para o _ehdr_ (a struct já existe em elf.h)
	fread(buffer, sizeof(char), sizeof(Elf32_Ehdr), *execfile);

	*ehdr = (Elf32_Ehdr *)buffer;

	// 3 - Com o ehdr já serializado, ler o _e_phoff_ para encontrar a Program Header Table
	e_phoff = ((Elf32_Ehdr *)*ehdr)->e_phoff;
	e_phentsize = ((Elf32_Ehdr *)*ehdr)->e_phentsize;
	e_phnum = ((Elf32_Ehdr *)*ehdr)->e_phnum;

	phtable_size = e_phentsize * e_phnum;

	// 4 - Ler o bloco onde se encontra a Program Header Table e armazenar no _buffer_
	fseek(*execfile, e_phoff, SEEK_SET);

	buffer = realloc(buffer, phtable_size);
	fread(buffer, sizeof(char), phtable_size, *execfile);
	// 5 - Criar a variável _pheader_ do tipo Elf32_Phdr*
	// 5 - Serializar o _buffer_ e passar para o _pheader_
	pheader = (Elf32_Phdr *)buffer;
	// 6 - Retornar _pheader_
	return pheader;
}

/* Writes the bootblock to the image file */
void write_bootblock(FILE **imagefile, FILE *bootfile, Elf32_Ehdr *boot_header, Elf32_Phdr *boot_phdr)
{
	// 1 - Pegar o tamanho do arquivo
	int mem_size = (boot_phdr)->p_memsz;

	// 2 - Posicionar a leitura no offset do bootfile
	fseek(bootfile, boot_phdr->p_offset, SEEK_SET);

	// 3 - Colocar os setores de programa num buffer
	char *buffer = (char *)malloc(sizeof(char) * mem_size);
	fread(buffer, sizeof(char), mem_size, bootfile);

	// 4 - Copiar o buffer para o o imagefile
	fwrite(buffer, sizeof(char), mem_size, *imagefile);

	// 1 - Calcular tamanho do bootblock sem o elfheader
	int boot_size = sizeof(*bootfile) - sizeof(Elf32_Ehdr);

	return;

	fclose(*imagefile);

	// 2 - Passar o bloco inteiro sem o elfheader para o imagefile
	fseek(bootfile, sizeof(Elf32_Ehdr), SEEK_SET);
	fread(buffer, sizeof(char), boot_size, bootfile);
	fwrite(buffer, sizeof(char), boot_size, *imagefile);
	//fclose(*imagefile);
}

/* Writes the kernel to the image file */
void write_kernel(FILE **imagefile, FILE *kernelfile, Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr)
{
	int mem_size = (kernel_phdr)->p_memsz;

	fseek(kernelfile, kernel_phdr->p_offset, SEEK_SET);
	char *buffer = (char *)malloc(sizeof(char) * mem_size);
	fread(buffer, sizeof(char), mem_size, kernelfile);
	fwrite(buffer, sizeof(char), mem_size, *imagefile);

	return;

	//

	int kernel_size = sizeof(*kernelfile); // - sizeof(Elf32_Ehdr);

	fseek(kernelfile, sizeof(Elf32_Ehdr), SEEK_SET);
	fread(buffer, sizeof(char), kernel_size, kernelfile);
	//fseek(*imagefile, 0, SEEK_END);
	fwrite(buffer, sizeof(char), kernel_size, *imagefile);
}

/* Counts the number of sectors in the kernel */
int count_kernel_sectors(Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr)
{
	return 0;
}

/* Records the number of sectors in the kernel */
void record_kernel_sectors(FILE **imagefile, Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr, int num_sec)
{
}

/* Prints segment information for --extended option */
void extended_opt(Elf32_Phdr *bph, int k_phnum, Elf32_Phdr *kph, int num_sec)
{

	/* print number of disk sectors used by the image */

	/* bootblock segment info */

	/* print kernel segment info */

	/* print kernel size in sectors */
}
// more helper functions...

/* MAIN */
int main(int argc, char **argv)
{
	FILE *kernelfile, *bootfile, *imagefile;				// file pointers for bootblock,kernel and image
	Elf32_Ehdr *boot_header = malloc(sizeof(Elf32_Ehdr));	// bootblock ELF header
	Elf32_Ehdr *kernel_header = malloc(sizeof(Elf32_Ehdr)); // kernel ELF header

	Elf32_Phdr *boot_program_header;   // bootblock ELF program header
	Elf32_Phdr *kernel_program_header; // kernel ELF program header

	/* build image file */
	imagefile = fopen("my_image", "wb+");
	assert(imagefile);

	/* read executable bootblock file */
	boot_program_header = read_exec_file(&bootfile, "bootblock", &boot_header);

	/* write bootblock */
	write_bootblock(&imagefile, bootfile, boot_header, boot_program_header);

	/* read executable kernel file */
	kernel_program_header = read_exec_file(&kernelfile, "kernel", &kernel_header);

	/* write kernel segments to image */
	write_kernel(&imagefile, kernelfile, kernel_header, kernel_program_header);

	/* close imagefile*/
	fclose(imagefile);

	/* tell the bootloader how many sectors to read to load the kernel */

	/* check for  --extended option */
	if (!strncmp(argv[1], "--extended", 11))
	{
		/* print info */
	}

	return 0;
} // ends main()
