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

/************USAGE***************/
void usage(char *progname) {
    fprintf(stderr, "usage: %s <imagename>\n", progname);
    exit(1);
}

/************PRINT_INDENT***************/
void print_indent(int indent)
{
    int i;
    for (i = 0; i < indent*4; i++)
	printf(" ");
}

/************GET_DIRENT***************/
uint16_t get_dirent(struct direntry *dirent, char *buffer)
{
    uint16_t followclust = 0;
    memset(buffer, 0, MAXFILENAME);

    int i;
    char name[9];
    char extension[4];
    uint16_t file_cluster;
    name[8] = ' ';
    extension[3] = ' ';
    memcpy(name, &(dirent->deName[0]), 8);
    memcpy(extension, dirent->deExtension, 3);
    if (name[0] == SLOT_EMPTY)
    {
	return followclust;
    }

    /* skip over deleted entries */
    if (((uint8_t)name[0]) == SLOT_DELETED)
    {
	return followclust;
    }

    if (((uint8_t)name[0]) == 0x2E)
    {
	// dot entry ("." or "..")
	// skip it
        return followclust;
    }

    /* names are space padded - remove the spaces */
    for (i = 8; i > 0; i--) 
    {
	if (name[i] == ' ') 
	    name[i] = '\0';
	else 
	    break;
    }

    /* remove the spaces from extensions */
    for (i = 3; i > 0; i--) 
    {
	if (extension[i] == ' ') 
	    extension[i] = '\0';
	else 
	    break;
    }

    if ((dirent->deAttributes & ATTR_WIN95LFN) == ATTR_WIN95LFN)
    {
	// ignore any long file name extension entries
	//
	// printf("Win95 long-filename entry seq 0x%0x\n", dirent->deName[0]);
    }
    else if ((dirent->deAttributes & ATTR_DIRECTORY) != 0) 
    {
        // don't deal with hidden directories; MacOS makes these
        // for trash directories and such; just ignore them.
	if ((dirent->deAttributes & ATTR_HIDDEN) != ATTR_HIDDEN)
        {
            strcpy(buffer, name);
            file_cluster = getushort(dirent->deStartCluster);
            followclust = file_cluster;
        }
    }
    else 
    {
        /*
         * a "regular" file entry
         * print attributes, size, starting cluster, etc.
         */
        strcpy(buffer, name);
        if (strlen(extension))  
        {
            strcat(buffer, ".");
            strcat(buffer, extension);
        }
    }

    return followclust;
}
/************PRINT_DIRENT***************/
uint16_t print_dirent(struct direntry *dirent, int indent, uint8_t *image_buf, struct bpb33* bpb, int *refcount )
{
    uint16_t followclust = 0;

    int i;
    char name[9];
    char extension[4];
    uint32_t size;
    uint16_t file_cluster;
    name[8] = ' ';
    extension[3] = ' ';
    memcpy(name, &(dirent->deName[0]), 8);
    memcpy(extension, dirent->deExtension, 3);
    if (name[0] == SLOT_EMPTY)
    {
	return followclust;
    }

    /* skip over deleted entries */
    if (((uint8_t)name[0]) == SLOT_DELETED)
    {
	return followclust;
    }

    if (((uint8_t)name[0]) == 0x2E)
    {
	// dot entry ("." or "..")
	// skip it
        return followclust;
    }

    /* names are space padded - remove the spaces */
    for (i = 8; i > 0; i--) 
    {
	if (name[i] == ' ') 
	    name[i] = '\0';
	else 
	    break;
    }

    /* remove the spaces from extensions */
    for (i = 3; i > 0; i--) 
    {
	if (extension[i] == ' ') 
	    extension[i] = '\0';
	else 
	    break;
    }

    if ((dirent->deAttributes & ATTR_WIN95LFN) == ATTR_WIN95LFN)
    {
	// ignore any long file name extension entries
	//
	// printf("Win95 long-filename entry seq 0x%0x\n", dirent->deName[0]);
    }
    else if ((dirent->deAttributes & ATTR_VOLUME) != 0) 
    {
	printf("Volume: %s\n", name);
    } 
    else if ((dirent->deAttributes & ATTR_DIRECTORY) != 0) 
    {
        // don't deal with hidden directories; MacOS makes these
        // for trash directories and such; just ignore them.
	if ((dirent->deAttributes & ATTR_HIDDEN) != ATTR_HIDDEN)
        {
	    print_indent(indent);
    	    printf("%s/ (directory)\n", name);
            file_cluster = getushort(dirent->deStartCluster);
            followclust = file_cluster;
        }
    }
    else 
    {
        /*
         * a "regular" file entry
         * print attributes, size, starting cluster, etc.
         */
	int ro = (dirent->deAttributes & ATTR_READONLY) == ATTR_READONLY;
	int hidden = (dirent->deAttributes & ATTR_HIDDEN) == ATTR_HIDDEN;
	int sys = (dirent->deAttributes & ATTR_SYSTEM) == ATTR_SYSTEM;
	int arch = (dirent->deAttributes & ATTR_ARCHIVE) == ATTR_ARCHIVE;

	size = getulong(dirent->deFileSize);
	print_indent(indent);
	printf("%s.%s (%u bytes) (starting cluster %d) %c%c%c%c\n", 
	       name, extension, size, getushort(dirent->deStartCluster),
	       ro?'r':' ', 
               hidden?'h':' ', 
               sys?'s':' ', 
               arch?'a':' ');

		
		int cluster_count = 0;
        
		uint16_t curr_cluster = getushort(dirent->deStartCluster);
		uint16_t original_cluster = curr_cluster;
        
        uint16_t previous;

        while(is_valid_cluster(curr_cluster,bpb))
		{
            refcount[curr_cluster]++;
			if(refcount[curr_cluster] > 1) //problem
			{
				printf("ERROR REFCOUNT > 1 FOR:  %d, refcount = %d\n", curr_cluster, refcount[curr_cluster]);  
				dirent->deName[0] = SLOT_DELETED;
				refcount[curr_cluster]--;
			}

			previous = curr_cluster;
			curr_cluster = get_fat_entry(curr_cluster, image_buf, bpb);
			if(previous == curr_cluster) //points to itself
			{
				printf("ERROR POINTS TO SELF\n");
				set_fat_entry(curr_cluster, FAT12_MASK & CLUST_EOFS, image_buf, bpb);
				cluster_count++;
				break;
			}
			if(curr_cluster == (FAT12_MASK & CLUST_BAD))
			{
				printf("BAD CLUSTER\n");
				set_fat_entry(curr_cluster, FAT12_MASK & CLUST_FREE, image_buf, bpb);
				set_fat_entry(previous, FAT12_MASK & CLUST_EOFS, image_buf, bpb);
				break;
			}
			cluster_count++;
			int clusters = 0;
			if(size%512 == 0)
			{
				clusters = size/512;
			}
			else
			{
				clusters = (size/512)+1;
			}
			printf("cluster_count = %d\n", cluster_count);
			printf("clusters = %d\n", clusters);
			if(clusters < cluster_count)
			{
				printf("ERROR: FILE SIZE TOO SMALL FOR NUM CLUSTERS\n");
				curr_cluster = get_fat_entry(original_cluster+clusters - 1, image_buf, bpb);
				while(is_valid_cluster(curr_cluster, bpb))
				{
					previous = curr_cluster;
					set_fat_entry(previous, FAT12_MASK & CLUST_FREE, image_buf, bpb);
					curr_cluster = get_fat_entry(curr_cluster, image_buf, bpb);
				}
				set_fat_entry(original_cluster +clusters -1, FAT12_MASK &CLUST_EOFS, image_buf, bpb);
			}
			else if(clusters > cluster_count)
			{
				printf("ERROR: FILE SIZE TOO LARGE FOR NUM CLUSTERS\n");
				uint32_t correct_size = cluster_count * bpb->bpbBytesPerSec;
				putulong(dirent->deFileSize, correct_size);
			}
			
							

	
		}
		

    }

    return followclust;
}

/************FOLLOW_DIR***************/
void follow_dir(uint16_t cluster, int indent, uint8_t *image_buf, struct bpb33* bpb, int* refcount)
{
    while (is_valid_cluster(cluster, bpb))
    {
        struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

        int numDirEntries = (bpb->bpbBytesPerSec * bpb->bpbSecPerClust) / sizeof(struct direntry); //can't we just use int numDirEntries = bpb->RootDirEnts??
        int i = 0;
	
	for ( ; i < numDirEntries; i++)
	{
           // char buffer[MAXFILENAME]; 
           // uint16_t followclust = get_dirent(dirent, buffer);
			uint16_t followclust = print_dirent(dirent, indent, image_buf, bpb, refcount);		//changed to print as follow_dir is called	
			
            if (followclust)
			{
				refcount[followclust]++;
                follow_dir(followclust, indent+1, image_buf, bpb, refcount);
			}
            dirent++;
	}

	cluster = get_fat_entry(cluster, image_buf, bpb);
    }
}

/***********CHECK_REFCOUNT***************/
//This function traverses the root directory looking for errors. It takes
// the image buffer, bpb, and a pointer to the array refcount. The function 
// will walk through the FAT updating refcounts, and checking for errors
// [maybe even one day it will fix them]

void checkandfix(uint8_t *image_buf, struct bpb33* bpb, int *refcount)
{
	uint16_t cluster = 0;
	struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);
	int i = 0;
    for ( ; i < bpb->bpbRootDirEnts; i++)
    {
		//to print directory use print_dirent (change to get_dirent when done)
        uint16_t followclust = print_dirent(dirent, 0, image_buf, bpb, refcount);
	
		//char buffer[MAXFILENAME]; 
        //uint16_t followclust = get_dirent(dirent, buffer);
        if (is_valid_cluster(followclust, bpb))
		{
			refcount[followclust]++; //updating refcount for index of current cluster
			printf("refcount is: %d, followclust is: %d\n\n",  refcount[followclust], followclust);
        	follow_dir(followclust, 1, image_buf, bpb, refcount);
        }


		//this is where the magic happens...or in print_dirent? I'd rather do 
		// it here
		
		

        dirent++;
    }

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

    
	//uint16_t cluster = 0;

   // struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

	int	FATsz = bpb->bpbSectors;
	int *refcount = (int *)malloc(sizeof(int)*FATsz);
	memset((int*)refcount, 0, FATsz * sizeof(int));  //creates array of all 0s of FAT size
	
	/*int i = 0;
    for ( ; i < bpb->bpbRootDirEnts; i++)
    { 
        uint16_t followclust = print_dirent(dirent,0, image_buf, bpb, refcount );
        if (is_valid_cluster(followclust, bpb))

            follow_dir(followclust, 1, image_buf, bpb, refcount);

        dirent++;
    }*/

	//where we run dope functions
	printf("\n");
	checkandfix(image_buf, bpb, refcount);

	
	






    unmmap_file(image_buf, &fd);
    return 0;
}
