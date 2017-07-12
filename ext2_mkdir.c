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
	if(temp[strlen(temp)-1] == '/'){
		temp[strlen(temp)-1] = '\0';}
	if(is_root(temp)){
		printf("Directory name already exists\n");//root already exists
		return EEXIST;
	}

	check_if_parent_exists(temp);
	if((path_exists != 0) || (i_type == 'f')){ //if parent directory doesn't exist or it's a file
		printf("Path does not exist\n");
		return ENOENT;
	}
	reset_traverse();

	traverse_path(temp);//check if directory/file already exists
	if(path_exists == 0){
		printf("Directory name already exists\n");
		return EEXIST;
	}
	reset_traverse();
	
	//first check if there is space on disk
	int empty_block = find_empty_block();
	int new_inode_num = find_unused_inode();
	const char *dir_name = get_name(temp); //new directory name

	//point to parent directory entry
	//traverse_index will be index of directory entry
	check_if_parent_exists(temp);
	struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *)(disk + EXT2_BLOCK_SIZE * dir_entries[traverse_index]);
	int parent_inode = dir->inode;
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
	dir->name_len = strlen(dir_name);
	dir->file_type = EXT2_FT_DIR;
	strncpy(dir->name,dir_name,strlen(dir_name));
	
	//point to new directory entry and inode
	struct ext2_dir_entry_2 *new_dir = (void *)disk + empty_block * EXT2_BLOCK_SIZE;
	struct ext2_inode *new_inode = ino+new_inode_num;
	
	//update current directory
	new_dir->inode = new_inode_num + 1;
	new_dir->rec_len = 12;
	new_dir->name_len = 1;
	new_dir->file_type = EXT2_FT_DIR;
	strncpy(new_dir->name,".",1);
	
	//update current directory's parent
	new_dir = (void *)new_dir + 12;
	new_dir->inode = parent_inode;
	new_dir->rec_len =  EXT2_BLOCK_SIZE-12;
	new_dir->name_len = 2;
	new_dir->file_type = EXT2_FT_DIR;
	strncpy(new_dir->name,"..",2);

	//updates written inode
	new_inode->i_block[0] = empty_block;
	new_inode->i_blocks = 2;
	new_inode->i_mode = EXT2_S_IFDIR;
	new_inode->i_links_count = 2;
	new_inode->i_size = EXT2_BLOCK_SIZE;
	
	g->bg_used_dirs_count++; //update dir count
	set_bitmaps(empty_block-1,new_inode_num);//sets bits to 1 in bitmaps
	
	return 0;
}
