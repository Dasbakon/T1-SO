/* Author(s): Felipe Lira de Oliveira, Matheus Lopo Borges
 * Creates operating system image suitable for placement on a boot disk
*/
/* Fully implemented according to the project description*/
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512				/* floppy sector size in bytes */
#define BOOTLOADER_SIG_OFFSET 0x1fe /* offset for boot loader signature */
#define MBR_SIGN 0x55aa				/* assinatura do MBR */
// more defines...

/* Reads in an executable file in ELF format*/
Elf32_Phdr *read_exec_file(FILE **execfile, char *filename, Elf32_Ehdr **ehdr)
{
	Elf32_Word e_phoff = 0;							  // offset da program header table
	Elf32_Half e_phentsize = 0;						  // tamanho da entrada na program header
	Elf32_Half e_phnum = 0;							  // numero de entradas na program header table
	Elf32_Phdr *pheader = malloc(sizeof(Elf32_Phdr)); // program header
	int phtable_size = 0;							  // tamanho da program header table em bytes
	char *buffer;									  // buffer para ler do arquivo e extrair os headers

	// abrir o arquivo
	*execfile = fopen(filename, "r");
	assert(execfile);

	// alocar buffer
	buffer = (char *)malloc(SECTOR_SIZE * sizeof(char));
	assert(buffer && "Nao foi possivel alocar o buffer\n");

	// Agora, com o _execfile_ aberto, temos que:
	// 1 - ler o primeiro bloco e armazenar em um _buffer_
	// 2 - Serializar o _buffer_ e passar para o _ehdr_ (a struct já existe em elf.h)
	fread(buffer, sizeof(char), sizeof(Elf32_Ehdr), *execfile);
	memcpy(*ehdr, (Elf32_Ehdr *)buffer, sizeof(Elf32_Ehdr));

	// 3 - Com o ehdr já serializado, ler os atributos para calcular o tamanho da Program Header Table
	e_phoff = ((Elf32_Ehdr *)*ehdr)->e_phoff;
	e_phentsize = ((Elf32_Ehdr *)*ehdr)->e_phentsize;
	e_phnum = ((Elf32_Ehdr *)*ehdr)->e_phnum;

	phtable_size = e_phentsize * e_phnum;

	// 4 - Ler o bloco onde se encontra a Program Header Table com o _buffer_
	fseek(*execfile, e_phoff, SEEK_SET);
	buffer = realloc(buffer, SECTOR_SIZE);
	fread(buffer, sizeof(char), phtable_size, *execfile);

	// 5 - Serializar o _buffer_ e passar para o _pheader_
	memcpy(pheader, (Elf32_Phdr *)buffer, sizeof(Elf32_Phdr));

	// 6 - Liberar o buffer
	free(buffer);

	// 7 - Retornar _pheader_
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
	char *buffer = (char *)malloc(sizeof(char) * SECTOR_SIZE);
	fread(buffer, sizeof(char), mem_size, bootfile);

	// 4 - Copiar o buffer para o o imagefile
	fwrite(buffer, sizeof(char), SECTOR_SIZE, *imagefile);

	// 5 - Liberar o buffer
	free(buffer);
}

/* Writes the kernel to the image file */
void write_kernel(FILE **imagefile, FILE *kernelfile, Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr)
{
	int mem_size = (kernel_phdr)->p_memsz;				// Tamanho na memória do segmento
	int sectors = ceil(mem_size / (double)SECTOR_SIZE); // Número de setores do kernel
	int last_sector_bytes = mem_size % SECTOR_SIZE;		// Overflow dos ultimos bytes que vão para um setor a mais

	// 1 - Alocar buffer, posicionar a leitura do kernel na posição do primeiro segmento de programa
	char *buffer = (char *)malloc(sizeof(char) * SECTOR_SIZE);
	fseek(kernelfile, kernel_phdr->p_offset, SEEK_SET);

	// 2 - Iterar pelos setores do kernel para escrevê-lo na imagem
	for (int i = 0; i < sectors; i++)
	{
		// 3 - Ler um setor com o buffer
		(i == (sectors - 1)) ? fread(buffer, sizeof(char), last_sector_bytes, kernelfile) : fread(buffer, sizeof(char), SECTOR_SIZE, kernelfile);

		// 4 - Passar buffer pra imagem
		fwrite(buffer, sizeof(char), SECTOR_SIZE, *imagefile);
	}

	// 5 - Liberar o buffer
	free(buffer);
}

/*Escrever a assinatura do MBR na imagem*/
void write_MBR_signature(FILE **imagefile)
{
	// 1 - Alocar buffer
	char *buffer = (char *)malloc(sizeof(char) * sizeof(short));

	// 2 - Converter em little-endian e passar ao buffer como short (int-16)
	*(short *)buffer = ((MBR_SIGN >> 8) & 0xff) | ((MBR_SIGN << 8) & 0xff00);

	// 3 - Posicionar escrita nos últimos bytes do setor de bootblock e escrever o MBR
	fseek(*imagefile, BOOTLOADER_SIG_OFFSET, SEEK_SET);
	fwrite(buffer, sizeof(char), sizeof(short), *imagefile);

	// 4 - Liberar o buffer
	free(buffer);
}

/* Counts the number of sectors in the kernel */
int count_kernel_sectors(Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr)
{
	Elf32_Half kernel_phnum = kernel_header->e_phnum; // Número de entradas na Program Header Table do krrnel

	//1 - Recebe a quantidade de setores do Kernel;
	int sectors = ceil((kernel_phdr->p_filesz * kernel_phnum) / (double)SECTOR_SIZE);

	return sectors;
}

/* Records the number of sectors in the kernel */
void record_kernel_sectors(FILE **imagefile, Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr, int num_sec)
{
	int *p_num_sec = &num_sec; // Número de setores do Kernel em ponteiro para int

	// 1 - Posicionar leitura da imagem na posição 2 e escrever o tamanho do kernel em setores
	fseek(*imagefile, 2, SEEK_SET);
	fwrite(p_num_sec, sizeof(int), 1, *imagefile);
}

/* Prints segment information for --extended option */
void extended_opt(Elf32_Phdr *bph, int k_phnum, Elf32_Phdr *kph, int num_sec)
{
	/* print number of disk sectors used by the image */

	/* bootblock segment info */
	printf("0x%04x: ./bootblock\n", bph->p_vaddr);
	printf("\tsegment 0\n");
	printf("\t\toffset 0x%04x\t", bph->p_offset);
	printf("vadrr 0x%04x\n", bph->p_vaddr);
	printf("\t\tfilesz 0x%04x\t", bph->p_filesz);
	printf("memsz 0x%04x\n", bph->p_memsz);
	printf("\t\twriting 0x%04x bytes\n", bph->p_filesz);
	printf("\t\tpadding up to 0x%04x\n", bph->p_vaddr + SECTOR_SIZE * 1);

	/* print kernel segment info */
	printf("0x%04x: ./kernel\n", kph->p_vaddr);
	printf("\tsegment 0\n");
	printf("\t\toffset 0x%04x\t", kph->p_offset);
	printf("vadrr 0x%04x\n", kph->p_vaddr);
	printf("\t\tfilesz 0x%04x\t", kph->p_filesz);
	printf("memsz 0x%04x\n", kph->p_memsz);
	printf("\t\twriting 0x%04x bytes\n", kph->p_filesz);
	printf("\t\tpadding up to 0x%04x\n", SECTOR_SIZE * (num_sec + 1));

	/* print kernel size in sectors * */
	printf("os_size: %d sectors\n", num_sec);
}
// more helper functions...

/* MAIN */
int main(int argc, char **argv)
{
	int numsec = 0;

	FILE *kernelfile, *bootfile, *imagefile;				// file pointers for bootblock,kernel and image
	Elf32_Ehdr *boot_header = malloc(sizeof(Elf32_Ehdr));	// bootblock ELF header
	Elf32_Ehdr *kernel_header = malloc(sizeof(Elf32_Ehdr)); // kernel ELF header

	Elf32_Phdr *boot_program_header;   // bootblock ELF program header
	Elf32_Phdr *kernel_program_header; // kernel ELF program header

	/* build image file */
	imagefile = fopen(IMAGE_FILE, "wb+");
	assert(imagefile);

	/* read executable bootblock file */
	boot_program_header = read_exec_file(&bootfile, argv[2], &boot_header);

	/* write bootblock */
	write_bootblock(&imagefile, bootfile, boot_header, boot_program_header);

	/* read executable kernel file */
	kernel_program_header = read_exec_file(&kernelfile, argv[3], &kernel_header);

	/* write kernel segments to image */
	write_kernel(&imagefile, kernelfile, kernel_header, kernel_program_header);

	/* write the MBR signature to image */
	write_MBR_signature(&imagefile);

	/* tell the bootloader how many sectors to read to load the kernel */
	numsec = count_kernel_sectors(kernel_header, kernel_program_header);
	record_kernel_sectors(&imagefile, kernel_header, kernel_program_header, numsec);

	/* close imagefile*/
	fclose(imagefile);
	/* check for  --extended option */
	if (!strncmp(argv[1], "--extended", 11))
	{
		/* print info */
		extended_opt(boot_program_header, (int)kernel_header->e_phnum, kernel_program_header, numsec);
	}

	return 0;
} // ends main()
