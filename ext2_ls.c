#include "ext2_utils.c"


int main(int argc, char **argv) {

    if(argc != 3) {
        printf("Needs 3 args\n");
        exit(1);
    }
    initialize_disk(argv[1]);
    traverse_path(argv[2]);//we are now in the right directory
	
	if(path_exists == -1){
		printf("No such file or directory\n");
		return ENOENT;
	}
	if (i_type == 'f') { //if its a file
		printf("No such directory exists\n");
		return(ENOTDIR);
	}

	//print directory entries in directory
    struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *)(disk + EXT2_BLOCK_SIZE * dir_entries[traverse_index]);
	char name[256];
	name[0] = '\0';
	
	int  block_rem = EXT2_BLOCK_SIZE;
	strncpy(name, dir->name, (dir->name_len));
	printf("%s\n", name);
	block_rem -= dir->rec_len;
	strncpy(name, "", strlen(name));
	while(block_rem > 0){
		dir = (void *)dir + (dir->rec_len);
		strncpy(name, dir->name, (dir->name_len));
		printf("%s\n", name);
		strncpy(name, "", strlen(name));
		block_rem -= dir->rec_len;
	}

    return 0;
}
