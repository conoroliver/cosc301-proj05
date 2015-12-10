#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "bootsect.h"
#include "bpb.h"
#include "direntry.h"
#include "fat.h"
#include "dos.h"

//Conor Oliver, Connor Van Cleave, Matt Condit

void usage(char *progname) {
    fprintf(stderr, "usage: %s <imagename>\n", progname);
    exit(1);
}


int main(int argc, char** argv) {
    uint8_t *image_buf;
    int fd;
    struct bpb33* bpb;
    if (argc < 2) {
	usage(argv[0]);
    }

    image_buf = mmap_file(argv[1], &fd);
    bpb = check_bootsector(image_buf);

    // your code should start here...

	int	FATsz = bpb->bpbSectors;
	int refcount[FATsz];
	memset(refcount, 0, FATsz * sizeof(int));  //creates array of all 0s of FAT size
	
/*	for(int i = 0; i < FATsz; i++)
	{
		printf("%d ", refcount[i]);
	}	
	
	printf("%s%d\n", "size = ", FATsz); */ //CHECK FOR REFCOUNT STUFF




    unmmap_file(image_buf, &fd);
    return 0;
}
