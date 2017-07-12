#include "ext2_utils.c"

int main(int argc, char **argv) {
    if(argc != 4) {
        printf("Needs 4 args\n");
        exit(1);
    }
	initialize_disk(argv[1]);//enables disk to be read
	char temp[256];
	temp[0] = '\0';
	strncat(temp, argv[3], strlen(argv[3])); //copies destination path to temp

	if(is_root(argv[2])||is_root(temp)){ //root is a directory
		printf("Location refers to a directory\n");
		return EISDIR;
	}

	//check if path to parent directory exists
	check_if_parent_exists(temp);
	if((path_exists != 0) || (i_type == 'f')){ //if parent directory doesn't exist or it's a file
		printf("Path does not exist\n");
		return ENOENT;
	}
	struct ext2_dir_entry_2 *parent = (struct ext2_dir_entry_2 *)(disk + EXT2_BLOCK_SIZE * dir_entries[traverse_index]); //parent directory entry
	int parent_inode = parent->inode;
	reset_traverse();

	//check if destination already exists
	traverse_path(temp);
	if (path_exists == 0){
		printf("Destination already exists\n");
		return EEXIST;
	}
	reset_traverse();

	traverse_path(argv[2]);//check if source file exists
	if((path_exists == 0) && (i_type == 'd')){
		printf("Source location refers to a directory\n");
		return EISDIR;
	}
	if(is_dir(argv[2])){ //traverse_path went to parent directory
		printf("source Path does not exist\n");
		return ENOENT;
	}
	int dest_inode; //inode of source
	char dname[256]; //directory entry names
	dname[0] = '\0';
	const char *file_name = get_name(argv[2]);//save source file name in file_name

	struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *)(disk + EXT2_BLOCK_SIZE * dir_entries[traverse_index]);
	int  block_rem = EXT2_BLOCK_SIZE;
	while(block_rem > 0){
		strncat(dname,dir->name,dir->name_len);
		if(strcmp(dname,file_name)==0){ //compares with file name
			break;
		}
		strncpy(dname,"",1);
		block_rem -= dir->rec_len;
		
		if(block_rem < 1){
			break;
		}
		dir = (void *)dir + (dir->rec_len);
	}
	dest_inode = dir->inode;
	(ino+(dir->inode)-1)->i_links_count += 1; //update links count for source file inode
	(ino+parent_inode-1)->i_links_count += 1; //update links count directory inode
	
	block_rem = EXT2_BLOCK_SIZE;
	while(block_rem > 0){
		block_rem -= parent->rec_len;
		if(block_rem < 1){
			break;
		}
		parent = (void *)parent + (parent->rec_len);
	}
	block_rem += parent->rec_len; //we need to update this number
	rec_padding((struct ext2_dir_entry_2 *)parent, 0); //rec_len of old "last directory entry" was modified
	block_rem -= parent->rec_len;
	
	const char *link_name = get_name(temp); //new file name is stored
	
	//dir will now point to new directory entry in parent directory
	parent = (void *)parent + (parent->rec_len);
	parent->inode = dest_inode;
	parent->rec_len = block_rem;
	parent->name_len = strlen(link_name);
	parent->file_type = EXT2_FT_REG_FILE;
	strncpy(parent->name,link_name,strlen(link_name));
		
	return 0;
}
