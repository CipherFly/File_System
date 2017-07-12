#include "ext2_utils.c"

int main(int argc, char **argv) {

	if(argc != 4) {
        printf("Needs 4 args\n");
        exit(1);
    }
    initialize_disk(argv[1]);
	FILE *cpfile = fopen(argv[2], "r");
    if (cpfile == NULL){
		printf("No such file in native OS\n");
		return ENOENT;
	}
	if(is_dir(argv[3])){  //target name ends with /
		printf("Invalid destination name\n");
		return EPERM;
	}
	if(is_root(argv[3])){ //root is a directory
		printf("Location refers to a directory\n");
		return EISDIR;
	}
	check_if_parent_exists(argv[3]);
	if((path_exists != 0) || (i_type == 'f')){ //if parent directory doesn't exist or it's a file
		printf("Path does not exist\n");
		return ENOENT;
	}
	reset_traverse();

	traverse_path(argv[3]);
	if(path_exists == 0){
		printf("Destination name already exists\n");
		return EEXIST;
	}
	reset_traverse();

	check_if_parent_exists(argv[3]); //go to parent directory
	const char *filename = get_name(argv[3]);

	//first check if there is space on disk
	int empty_block = find_empty_block();
	int new_inode_num = find_unused_inode();

	fseek(cpfile, 0L, SEEK_END); 
	long int cpfile_size = ftell(cpfile); //size of file in native
	fseek(cpfile,0L, SEEK_SET);
	
	int number_of_blocks = 0;
	if ((cpfile_size % EXT2_BLOCK_SIZE) != 0){number_of_blocks++;}
	number_of_blocks += (cpfile_size / EXT2_BLOCK_SIZE); //number of blocks needed for cpfile
	if(number_of_blocks > 12){
		number_of_blocks++; //indirect block also takes up a block
	}
	is_there_space(number_of_blocks); //checks if disk has space

	struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *)(disk + EXT2_BLOCK_SIZE * dir_entries[traverse_index]);
	(ino+(dir->inode)-1)->i_links_count += 1; //update links count

	int  block_rem = EXT2_BLOCK_SIZE;
	while(block_rem > 0){
		block_rem -= dir->rec_len;
		if(block_rem < 1){
			break;
		}
		dir = (void *)dir + (dir->rec_len);
	}
	block_rem += dir->rec_len; //we need to update this number
	rec_padding((struct ext2_dir_entry_2 *)dir, 0); //rec_len of old "last directory entry" was modified
	block_rem -= dir->rec_len;
	
	//dir will now point to new directory entry in parent directory
	dir = (void *)dir + (dir->rec_len);
	dir->inode = new_inode_num + 1;
	dir->rec_len = block_rem;
	dir->name_len = strlen(filename);
	dir->file_type = EXT2_FT_REG_FILE;
	strncpy(dir->name,filename,strlen(filename));
	
	struct ext2_inode *new_inode = ino+new_inode_num;
	//update written inode
	new_inode->i_blocks = number_of_blocks*2;
	new_inode->i_mode = EXT2_S_IFREG;
	new_inode->i_links_count = 2;
	new_inode->i_size = cpfile_size;
	set_bitmaps(-1, new_inode_num);

	if(number_of_blocks > 12){ //if we need one level of indirection
		number_of_blocks = 13;
	}
	
	int i,t;
	char *copy;
	for(i = 0; i < number_of_blocks; i++){
		empty_block = find_empty_block();
		new_inode->i_block[i] = empty_block;
		set_bitmaps(empty_block-1, -1);
		copy = (void *)disk + empty_block * EXT2_BLOCK_SIZE;
		if(i <= 11){
			
			for(t = 0; t < EXT2_BLOCK_SIZE; t++){
				if(cpfile_size <= 0){
					break;}
				fread(&copy[t], 1, 1, cpfile);
				cpfile_size--;
			}
		}
		if(i == 12){
			int indirect = empty_block;//the indirect block is also a pointer
			int y; //each indirect pointer is 4 bytes
			for(y=0; y<256; y++){ //1024 / 4
				if(cpfile_size <= 0){
					*(disk + indirect*EXT2_BLOCK_SIZE + (y*4)) = 0;
				}
				else{
					empty_block = find_empty_block();
					set_bitmaps(empty_block-1,-1);
					*(disk + indirect*EXT2_BLOCK_SIZE + (y*4)) = empty_block;
					copy = (void *)disk + empty_block * EXT2_BLOCK_SIZE;
					for(t = 0; t < EXT2_BLOCK_SIZE; t++){
						if(cpfile_size <= 0){
							break;}
						fread(&copy[t], 1, 1, cpfile);
						cpfile_size--;
					}
				}
			}
		}
	}
	
	return 0;
}
