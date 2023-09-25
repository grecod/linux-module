/*
 * devmem2.c: Simple program to read/write from/to any location in memory.
 *
 *  Copyright (C) 2000, Jan-Derk Bakker (jdb@lartmaker.nl)
 *
 *
 * This software has been developed for the LART computing board
 * (http://www.lart.tudelft.nl/). The development has been sponsored by
 * the Mobile MultiMedia Communications (http://www.mmc.tudelft.nl/)
 * and Ubiquitous Communications (http://www.ubicom.tudelft.nl/)
 * projects.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>

#ifdef __LP64__
#define MEM_SUPPORT_64BIT_DATA	1
#else
#define MEM_SUPPORT_64BIT_DATA	0
#endif

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
  __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)
 
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

#define DISP_LINE_LEN	16
#define MAX_LINE_LENGTH_BYTES (64)
#define DEFAULT_LINE_LENGTH_BYTES (16)

int print_buffer(ulong addr, ulong phyaddr, const void *data, uint width, uint count,
		 uint linelen)
{
	/* linebuf as a union causes proper alignment */
	union linebuf {
		u_int64_t uq[MAX_LINE_LENGTH_BYTES/sizeof(u_int64_t) + 1];
		u_int32_t ui[MAX_LINE_LENGTH_BYTES/sizeof(u_int32_t) + 1];
		u_int16_t us[MAX_LINE_LENGTH_BYTES/sizeof(u_int16_t) + 1];
		u_int8_t  uc[MAX_LINE_LENGTH_BYTES/sizeof(u_int8_t) + 1];
	} lb;
	int i;
	ulong x;

	if (linelen*width > MAX_LINE_LENGTH_BYTES)
		linelen = MAX_LINE_LENGTH_BYTES / width;
	if (linelen < 1)
		linelen = DEFAULT_LINE_LENGTH_BYTES / width;

	while (count) {
		uint thislinelen = linelen;
		printf("0x%08lX (0x%08lX):", phyaddr, addr);

		/* check for overflow condition */
		if (count < thislinelen)
			thislinelen = count;

		/* Copy from memory into linebuf and print hex values */
		for (i = 0; i < thislinelen; i++) {
			if (width == 4)
				x = lb.ui[i] = *(volatile u_int32_t *)data;
			else if (MEM_SUPPORT_64BIT_DATA && width == 8)
				x = lb.uq[i] = *(volatile u_long *)data;
			else if (width == 2)
				x = lb.us[i] = *(volatile u_int16_t *)data;
			else
				x = lb.uc[i] = *(volatile u_int8_t *)data;

			printf(" %0*lX", width * 2, x);

			data += width;
		}

		while (thislinelen < linelen) {
			/* fill line with whitespace for nice ASCII print */
			for (i=0; i<width*2+1; i++)
				puts(" ");
			linelen--;
		}

		/* Print data in ASCII characters */
		for (i = 0; i < thislinelen * width; i++) {
			if (!isprint(lb.uc[i]) || lb.uc[i] >= 0x80)
				lb.uc[i] = '.';
		}
		lb.uc[i] = '\0';
		printf("    %s\n", lb.uc);

		/* update references */
		addr += thislinelen * width;
		phyaddr += thislinelen * width;
		count -= thislinelen;
	}

	return 0;
}


int main(int argc, char **argv) {
    int fd, lenth;
    void *map_base, *virt_addr; 
	unsigned long read_result, writeval;
	off_t target;
	int access_type = 'w';
	
	if(argc < 2) {
		fprintf(stderr, "\nUsage:\t%s { address } [ type [ data ] ]\n"
			"\taddress : memory address to act upon\n"
			"\ttype    : access operation type : [b]yte, [h]alfword, [w]ord\n"
			"\tdata    : data to be written\n"
			"\tmem dump: %s {address d} [ wordlength (<=1024) ]\n",
			argv[0], argv[0]);
		exit(1);
	}
	target = strtoul(argv[1], 0, 0);

	if(argc > 2)
		access_type = tolower(argv[2][0]);

    if((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) FATAL;
    printf("/dev/mem opened.\n"); 
    fflush(stdout);
    
    /* Map one page */
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target & ~MAP_MASK);
    if(map_base == (void *) -1) FATAL;
    
    virt_addr = map_base + (target & MAP_MASK);
	if (access_type == 'd') {
		lenth = 64;
		if(argc > 3) {
			lenth = strtoul(argv[3], 0, 0);
			lenth = ((lenth+3)/4)*4;
			if (lenth > 1024-(virt_addr-map_base)/4)
				lenth = 1024-(virt_addr-map_base)/4;
		}
		printf("Memory mapped at Base: %lX Start: %lX Length: %d Words\n", map_base, virt_addr, lenth);

		/* Print the lines. */
		print_buffer((ulong)virt_addr, target, virt_addr, 4, lenth, DISP_LINE_LEN / 4);
		goto exit;
	}

	printf("Memory mapped at address %p.\n", map_base);
    fflush(stdout);
	
    switch(access_type) {
		case 'b':
			read_result = *((unsigned char *) virt_addr);
			break;
		case 'h':
			read_result = *((unsigned short *) virt_addr);
			break;
		case 'w':
			read_result = *((unsigned long *) virt_addr);
			break;
		default:
			fprintf(stderr, "Illegal data type '%c'.\n", access_type);
			exit(2);
	}
    printf("Read at address  0x%X (%p): 0x%08X\n", target, virt_addr, read_result); 
    fflush(stdout);

	if(argc > 3) {
		writeval = strtoul(argv[3], 0, 0);
		switch(access_type) {
			case 'b':
				*((unsigned char *) virt_addr) = writeval;
				read_result = *((unsigned char *) virt_addr);
				break;
			case 'h':
				*((unsigned short *) virt_addr) = writeval;
				read_result = *((unsigned short *) virt_addr);
				break;
			case 'w':
				*((unsigned long *) virt_addr) = writeval;
				read_result = *((unsigned long *) virt_addr);
				break;
		}
		//printf("Written 0x%X; readback 0x%08X\n", writeval, read_result); 
		printf("Write at address 0x%X (%p): 0x%08X, readback 0x%08X\n",target, virt_addr, writeval, read_result); 
		fflush(stdout);
	}

exit:	
	if(munmap(map_base, MAP_SIZE) == -1) FATAL;
    close(fd);
    return 0;
}

