#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>


unsigned char *disk;


int main(int argc, char **argv) {

	


    if(argc != 2) {
        fprintf(stderr, "Usage: readimg <image file name>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    //char *abs_path = argv[2]; //absolute path

    int block_size = 1024; //size of individual block

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	perror("mmap");
	exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + block_size); 
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    printf("Block group:\n           ");
    struct ext2_group_desc *g = (struct ext2_group_desc *)(disk + (block_size * 2));
    printf("block bitmap: %d\n", g->bg_block_bitmap);
    printf("           inode bitmap: %d\n", g->bg_inode_bitmap);
    printf("           inode table: %d\n", g->bg_inode_table);
    printf("           free blocks: %d\n", g->bg_free_blocks_count);
    printf("           free inodes: %d\n", g->bg_free_inodes_count);
    printf("           used dirs: %d\n", g->bg_used_dirs_count);
	printf("           sb->s_log_block_size: %d\n", 2<<(sb->s_log_block_size));
    int byte;
	int bit;

	//block bitmap
    for (byte = 0; byte < 16; byte++) {
        for (bit = 0; bit < 8; bit++) {
			//print from left to right bits. 
            printf("%d", *(disk+(g->bg_block_bitmap*block_size+byte))>>bit & 1);
        }
        printf(" ");
    }
    printf("\n\n\n");


	//inode bitmap. Bit array.
    int inode_bitmap[sb->s_inodes_count];
    for (byte = 0; byte < 4; byte++) {
        for (bit = 0; bit < 8; bit++) {
			//populate inode_bitmap array. print from left to right bits.
            inode_bitmap[(8*byte) + bit] = *(disk+(g->bg_inode_bitmap*block_size+byte))>>bit & 1;
            printf("%d", inode_bitmap[(8*byte) + bit]);
        }
        printf(" ");
    }
    printf("\n");


    struct ext2_inode *ino = (struct ext2_inode *)(disk + (block_size * g->bg_inode_table));
    

    char type;
    
	int dir_entries[128];
	int dir_index = 0;
	int inode_entries[32];
	int x;
	for(x=0;x<32;x++){
		inode_entries[x]=0;
	}

	
	int i,j;
	



	//inodes
    for (i = 0; i < sb->s_inodes_count; i++) {
        if (inode_bitmap[i] == 1) {
            if (i == 1 || i >= 11) { //root is inode 2, and first 11 inodes are reserved
                if ((ino+i)->i_mode & EXT2_S_IFREG) {
                	type = 'f';
            	}
            	if ((ino+i)->i_mode & EXT2_S_IFDIR) {
					//if directory
                	type = 'd';
			

            	}
				//indexes start at 1, not 0
            	printf("[%d] type: %c size: %d liunks: %d blocks: %d\n", i+1, type, (ino+i)->i_size, (ino+i)->i_links_count, (ino+i)->i_blocks);
	    		if((ino+i)->i_blocks > 0){
					printf("[%d]   Blocks:     ", (i+1));


					int max_index = (ino+i)->i_blocks/(2<<sb->s_log_block_size);
					
					
					for(j=0; j<max_index; j++){
						//direct pointers
						if(j<=11){
							printf(" %d\n", (ino+i)->i_block[j]);
						}
						//indirect pointers, only one level of indirection
						if(j == 12){
							int indirect = (ino+i)->i_block[j];
							int y;
							for(y=0; y<256; y++){
								if(*(disk + indirect*block_size + y) != 0) {
									printf("                 %d\n", *(disk + indirect*block_size + y));
								}
							}
						}
					}
					if(type =='d'){
						inode_entries[dir_index] = i+1;
						dir_entries[dir_index] = (ino+i)->i_block[0];
						dir_index++;
					}






				}
        	}
		}
    }
    
	
	char ftype;
	
	printf("\n\nDirectory Blocks:");

	for (i = 0; i < dir_index; i++) {
		
		char fname[256];
		strncpy(fname, "", 256);
	

		printf("\n    DIR BLOCK NUM: %d (for inode %d)", dir_entries[i], inode_entries[i]);
		struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *)(disk + block_size * dir_entries[i]);
		int  block_rem = block_size - dir->rec_len;
		ftype = 'd';
		printf("\nInode: %u  rec_len: %u  name_len: %u  type: %c  name: %s",
				dir->inode, dir->rec_len, dir->name_len, ftype, dir->name);

		while(block_rem > 0){
			dir = (void *)dir + (dir->rec_len);
			if (dir->file_type & EXT2_FT_DIR) {
                		ftype = 'd';
            		}
            		else{
				ftype = 'f';
			}
			strncpy(fname, dir->name, (dir->name_len));
			printf("\nInode: %u  rec_len: %u  name_len: %u  type: %c  name: %s",
				dir->inode, dir->rec_len, dir->name_len, ftype, fname);
			strncpy(fname, "", (dir->name_len));
			block_rem -= dir->rec_len;
		}



	}


	



			


	printf("\n");


    return 0;
}
