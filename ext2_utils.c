#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include <errno.h>

#define NUM_INODES		32

unsigned char *disk;

int *inode_bitmap;

struct ext2_inode *ino;

struct ext2_super_block *sb;

struct ext2_group_desc *g;

int path_exists = 0; //0 if path exists, -1 if it doesn't

int dir_entries[NUM_INODES]; //data block index array
int inode_entries[NUM_INODES]; //inode index array

int traverse_index = 0; //index used to traverse directories

char i_type = 'd'; //type of most recent inode. root starts off as d


void initialize_disk(char *filename){
	int fd = open(filename, O_RDWR);
	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
    	}

	//initialize global structs
	sb = malloc(sizeof(struct ext2_super_block));
	sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	g = malloc(sizeof(struct ext2_group_desc));
	g = (struct ext2_group_desc *)(disk + (EXT2_BLOCK_SIZE * 2));
	ino = malloc(sizeof(struct ext2_inode));
	ino = (struct ext2_inode *)(disk + (EXT2_BLOCK_SIZE * g->bg_inode_table));

	int byte;
	int bit;
	//inode bitmap. Bit array.
	inode_bitmap = malloc(sizeof(int) * sb->s_inodes_count);
	for (byte = 0; byte < 4; byte++) {
        	for (bit = 0; bit < 8; bit++) {
			//populate inode_bitmap array.
			inode_bitmap[(8*byte) + bit] = *(disk+(g->bg_inode_bitmap*EXT2_BLOCK_SIZE+byte))>>bit & 1;
        	}
	}

	//variables used to iterate through inode blocks
	int i;
	int dir_index=0; //index for dir_entries
	char type;

	//inodes
    for (i = 0; i < sb->s_inodes_count; i++) {
        if (inode_bitmap[i] == 1) {
            if (i == 1 || i >= 11) { //root is inode 2, and first 11 inodes are reserved
				//regular file
                if ((ino+i)->i_mode & EXT2_S_IFREG) {
                	type = 'f';
            	}
				//directory
            	if ((ino+i)->i_mode & EXT2_S_IFDIR) {
                	type = 'd';
            	}
				//inode blocks
				//indexes start at 1, not 0
	    		if((ino+i)->i_blocks > 0){
					//log directory entries in array
					if(type =='d'){
						inode_entries[dir_index] = i+1;
						dir_entries[dir_index] = (ino+i)->i_block[0];//stores location of data block
						dir_index++;
					}

				}

        	}
		}
    }
	//end of inode iteration

	return;
}

//changes index of dir_entries
void change_traverse_index(int ino_number){
	if(i_type == 'f'){
		traverse_index = -1;
		return;
	}
	int i;
	for(i = 0; i < NUM_INODES; i++){
		if(inode_entries[i] == ino_number){
			traverse_index = i;
			return;
		}
	}
	return;
}

//returns inode number of directory if it exists, -1 otherwise
int check_if_path_exists(char *path){
	//directory entry
	struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *)(disk + EXT2_BLOCK_SIZE * dir_entries[traverse_index]);
	int  block_rem = EXT2_BLOCK_SIZE;  //remaining space in block
	char name[256];
	name[0]='\0';
	strncat(name,dir->name,dir->name_len);
	if(strcmp(name, path) == 0){
		return dir->inode;
	}
	block_rem -= dir->rec_len;
	strncpy(name,"",1);
	while(block_rem > 0){
		dir = (void *)dir + (dir->rec_len);
		strncat(name,dir->name,dir->name_len);
		if(strcmp(name, path) == 0){
			
			if ((ino+dir->inode-1)->i_mode & EXT2_S_IFREG){
				i_type = 'f';
				return -1;	//return -1 because its a file
			}
			else{
				i_type = 'd';
			}
			return dir->inode;
		}
		strncpy(name,"",1);
		block_rem -= dir->rec_len;
	}
	path_exists = -1; //path does not exist
	return -1;
}

//traverse_index will be the index in dir_entries
//path_exists will be changed
//i_type will show the type of inode
void traverse_path(char *fullpath) {
	if(fullpath[0] != '/'){
		printf("enter an absolute path\n");
		exit(1);
	}
	//add / to the end if it doesn't have it
	char path[256];
	path[0]='\0';
	strncat(path,fullpath,strlen(fullpath));
	if(path[strlen(path)-1] != '/'){
		strncat(path,"/",1);
	}

	int p_index = 1; //path index, we don't check if root exists
	char temp[256];
	temp[0] = '\0';
	int dir_inode;//inode of directory

	while(p_index < strlen(path)){
		if(path[p_index] == '/'){
			dir_inode = check_if_path_exists(temp);
			if(dir_inode == -1){
				return; //path does not exist
			}
			change_traverse_index(dir_inode);
			strncpy(temp,"",1); //clear temp
		}
		else{
			strncat(temp, &path[p_index], 1);
		}
		p_index++;
	}
	
	return;
}

//returns an empty data block if there are any
int find_empty_block(void){
	int x;
	int y;
	for (x = 0; x < 16; x++) {
        for (y = 0; y < 8; y++) {
        	if (!(*(disk+(g->bg_block_bitmap*EXT2_BLOCK_SIZE+x))>>y & 1)){
				return ((x*8)+y)+1;
			}
        }
    }
	printf("No space left on device\n");
	exit(ENOSPC);
}

//returns unused inode number
unsigned int find_unused_inode(void){
	int i;
	for (i = 11; i < sb->s_inodes_count; i++) {
        if (inode_bitmap[i] == 0) {
			return i;
		}
	}
	printf("No space left on device\n");
	exit(ENOSPC);
}

//changes the rec_len of directory entry to make room for new directory entry if op is 0
//otherwise increases padding until 1024 bytes (the block is filled). op would be block length remainding to fill
void rec_padding(struct ext2_dir_entry_2 *directory_entry, int op){
	if(op == 0){
		unsigned short new_rec = 8 + directory_entry->name_len;
		new_rec += 4- (new_rec % 4);
		directory_entry->rec_len = new_rec;
		return;
	}
	directory_entry->rec_len += op;
	return;
}


//block and inode num are index in bitmaps
//if inode or block_num is -1, do not set bit
void set_bitmaps(int block_num, int inode_num){
	if(block_num > -1){
		int x = (block_num / 8); //byte
		int y = block_num % 8; //bit
	
		unsigned char *bbitmap = (unsigned char *)(disk + (EXT2_BLOCK_SIZE * g->bg_block_bitmap));
		bbitmap += x;
		*bbitmap = (*bbitmap) | (1 << y);

		g->bg_free_blocks_count -= 1;
		sb->s_free_blocks_count -= 1;
	}
	if(inode_num > -1){
		int a = inode_num / 8; //byte
		int b = inode_num % 8; //bit

		unsigned char *ibitmap = (unsigned char *)(disk + (EXT2_BLOCK_SIZE * g->bg_inode_bitmap));
		ibitmap += a;
		*ibitmap = (*ibitmap) | (1 << b);

		g->bg_free_inodes_count -= 1;
		sb->s_free_inodes_count -= 1;
	}

	
	

	return;
}

//block and inode num are index in bitmaps
//if inode or blocknum is -1, do not uset bit
void unset_bitmaps(int block_num, int inode_num){
	if(block_num > -1){
		int x = (block_num / 8); //byte
		int y = block_num % 8; //bit
		unsigned char bmask = ~(1<<y); //unset bit

		unsigned char *bbitmap = (unsigned char *)(disk + (EXT2_BLOCK_SIZE * g->bg_block_bitmap));
		bbitmap += x;
		*bbitmap = (*bbitmap) & bmask;

		g->bg_free_blocks_count += 1;
		sb->s_free_blocks_count += 1;

	}
	if(inode_num > -1){
		int a = inode_num / 8; //byte
		int b = inode_num % 8; //bit
		unsigned char imask = ~(1<<b); //unset bit

		unsigned char *ibitmap = (unsigned char *)(disk + (EXT2_BLOCK_SIZE * g->bg_inode_bitmap));
		ibitmap += a;
		*ibitmap = (*ibitmap) & imask;
		g->bg_free_inodes_count += 1;
		sb->s_free_inodes_count += 1;
	}
	return;
}

//check if path to parent directory exists
void check_if_parent_exists(char *path){
	char directory_name[256]; //local variable so path isn't overwritten
	directory_name[0] = '\0';
	strncpy(directory_name, path, strlen(path));
	int x = strlen(directory_name)-1;
	while((directory_name[x] != '/') && (x>0)){
		x--;
	}
	directory_name[x+1] = '\0';
	traverse_path(directory_name);
	return;
}

//allows traverse_path to be used again
void reset_traverse(void){
	traverse_index = 0; //set directory entry index back to 0
	i_type = 'd'; //set type of inode back to directory
	path_exists = 0; //path now exists
	return;
}

//1 if root, 0 otherwise
int is_root(char *path){
	int x = 0;
	if(strncmp(path,"/",strlen(path)) == 0){
		x = 1;
	}	
	return x;
}

//1 if dir, 0 otherwise. Checks if last character in path is /
int is_dir(char *path){
	int x = 0;
	if(strncmp(&path[strlen(path)-1],"/",1) == 0){
		x = 1;
	}	
	return x;
}

//gets file/directory name from path
const char *get_name(char *path){
	char name[256];
	name[0] = '\0';
	int x = strlen(path)-1;
	while((path[x] != '/') && (x>0)){
		x--;
	}
	return &path[x+1];
}

void is_there_space(int number_of_blocks){ //checks if there is space on disk
	int x;
	int y;
	int space = 0;
	for (x = 0; x < 16; x++) {
        for (y = 0; y < 8; y++) {
        	if (!(*(disk+(g->bg_block_bitmap*EXT2_BLOCK_SIZE+x))>>y & 1)){
				space++;
			}
        }
    }
	if(number_of_blocks > space){
		printf("No space left on device\n");
		exit(ENOSPC);
	}
	return;
}
