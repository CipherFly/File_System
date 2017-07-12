#include "ext2_utils.c"

int main(int argc, char **argv) {
    if(argc != 3) {
        printf("Needs 3 args\n");
        exit(1);
    }
	initialize_disk(argv[1]);//enables disk to be read

	char temp[256];
	temp[0] = '\0';
	strncat(temp, argv[2], strlen(argv[2])); //copies path to temp

	traverse_path(temp);//check if path exists
	if(path_exists == -1){
		printf("No such file or directory\n");
		return ENOENT;
	}
	if(i_type == 'd'){ //if directory
		printf("Cannot delete directory\n");
		return EISDIR;
	}
	if(is_dir(temp)){ //traverse path went to parent directory
		printf("Directory doesn't exist\n");
		return ENOENT;
	}
	const char *file_name = get_name(temp); //file name to be deleted.

	//point to parent directory entry
	//traverse_index is set directory entry index of parent directory
	struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *)(disk + EXT2_BLOCK_SIZE * dir_entries[traverse_index]);

	char dname[256]; //directory entry names
	dname[0] = '\0';
	int temp_rec = dir->rec_len; //will store rec_len of last directory entry, for padding in the end

	int  block_rem = EXT2_BLOCK_SIZE;
	while(block_rem > 0){
		strncat(dname,dir->name,dir->name_len);
		if(strcmp(dname,file_name)==0){ //compares with file name
			break;
		}
		strncpy(dname,"",1);
		block_rem -= dir->rec_len;
		temp_rec = dir->rec_len;
		if(block_rem < 1){
			break;
		}
		dir = (void *)dir + (dir->rec_len);
	}

	//dir now points to directory entry to be deleted (the file). block_rem has length of block left.
	struct ext2_inode *rem_inode = ino+(dir->inode)-1;//inode to be potentially removed
	int rem_inode_num = dir->inode;  //inode number to be potentially removed
	int temp_rem = block_rem; //store block length remaining for padding

	//remove dir entry from parent
	//all dir entries will point to the next dir entry if it exists.
	while(block_rem > 0){
		block_rem -= dir->rec_len;
		if(block_rem < 1){
			dir = (void *)dir - temp_rec;//dir is now new "last directory entry"
			break;
		}
		int remove = dir->rec_len / 4; //dir entries are multiples of 4 bytes.
		int *dir_next = (void *)dir; //this will be the pointer to the next 4 bytes
		int *current = (void *)dir; //this is pointer to current 4 bytes
		int w;
		for(w=0;w<(block_rem-1);w++){ //copy over the deleted entry
			dir_next++;
			*current = *(dir_next+remove-1);
			current++;
		}
		int t = 0;//now go to last directory entry in parent directory
		while(block_rem > 0){
			block_rem -= dir->rec_len;
			t = dir->rec_len;
			dir = (void *)dir + dir->rec_len;
			temp_rem -= dir->rec_len;
		}
		dir = (void *)dir - t; //dir will point to the last directory entry in parent
		temp_rem = remove * 4;
		break;
	}
	rec_padding((struct ext2_dir_entry_2 *)dir, temp_rem); //change padding for last directory entry

		
	if(rem_inode->i_links_count == 1){
		int max_index = rem_inode->i_blocks/(2<<sb->s_log_block_size);
		int i = 0;
		for(i=0; i<max_index; i++){
			//direct pointers
			if(i<=11){
				unset_bitmaps(rem_inode->i_block[i]-1,-1);
				rem_inode->i_block[i] = 0;
			}
			//indirect pointers, only one level of indirection
			if(i == 12){
				int indirect = rem_inode->i_block[i];
				
				unset_bitmaps(indirect-1,-1); //the indirect block is also a pointer
				int y; //each indirect pointer is 4 bytes
				for(y=0; y<256; y++){ //1024 / 4
					if(*(disk + indirect*EXT2_BLOCK_SIZE + y) != 0) {
						unset_bitmaps(*(disk + indirect*EXT2_BLOCK_SIZE + y)-1,-1);
						*(disk + indirect*EXT2_BLOCK_SIZE + y) = 0;
					}
				}
				rem_inode->i_block[i] = 0; //set indirect pointer equal to zero
			}
		}
		unset_bitmaps(-1,rem_inode_num-1);
	}
	else{
		rem_inode->i_links_count -= 1; //update links count
	}
	
	return 0;
}
