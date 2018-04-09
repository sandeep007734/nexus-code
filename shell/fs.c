/* 
 * Copyright (c) 2017, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <nexus.h>
#include <nexus_log.h>
#include <nexus_config.h>
#include <nexus_volume.h>
#include <nexus_fs.h>
#include <nexus_util.h>

static int cmd_line_user_key = 0;





/* 
 * This modifies argv in place moving all the nexus options to the beginning, 
 * The return value is the number of options we capture as nexus options. 
 * Further option processing can happen using argv and the return value as the offset at which to start searching
 */
static int
__parse_nexus_options(int argc, char ** argv)
{
    int  opt_index = 0;
    int  used_opts = 0;
    char c         = 0;
    
    static struct option long_options[] = {
	{ "user_key" , required_argument , &cmd_line_user_key ,  1  },  /* 0 */
	{ 0, 0, 0, 0 }
    };
    
    
    while ((c = getopt_long_only(argc, argv, "h", long_options, &opt_index)) != -1) {	    
	
	switch (c) {
	    case 0:
		switch (opt_index) {
		    case 0:
			nexus_config.user_key_path = optarg;
			used_opts += 2;
			break;
			
			
		    default:
			return -1;
		}
		break;
	    case 'h':
	    case '?':
	    default:
		return -1;
	}
    }
    
    return used_opts;
}


static void create_file_usage()
{
    printf("create_file: create a file at a volume path\n\n"\
	   "Usage: create_file <volume> <path-in-volume>\n");

    return;
}

static int
__fs_touch(struct nexus_volume * vol, const char * path, nexus_dirent_type_t type)
{
    char * dirpath  = NULL;
    char * filename = NULL;

    char * nexus_name = NULL;

    int ret = -1;

    nexus_splitpath(path, &dirpath, &filename);

    ret = nexus_fs_touch(vol, dirpath, filename, type, &nexus_name);

    nexus_free(dirpath);
    nexus_free(filename);

    if (ret != 0) {
	log_error("creating %s FAILED\n", path);
	return -1;
    }

    printf(" .created %s -> %s\n", path, nexus_name);
    fflush(stdout);

    nexus_free(nexus_name);

    return 0;
}


int
create_file_main(int argc, char ** argv)
{
    struct nexus_volume * vol = NULL;

    struct nexus_stat file_stat;

    char * volume_path  = NULL;
    char * file_path    = NULL;

    int    used_opts    = 0;
    int    ret          = 0;

    
    used_opts = __parse_nexus_options(argc, argv);

    if (used_opts == -1) {
	create_file_usage();
	return -1;
    }
    
		
    /* At this point we should just have the volume path in ARGV */
    
    if ((argc - used_opts) != 3) {
	create_file_usage();
	return -1;
    }
    
    
    volume_path = argv[used_opts + 1];
    file_path   = argv[used_opts + 2];
    
    
    vol = nexus_mount_volume(volume_path);

    if (vol == NULL) {
	printf("Error: could not mount nexus volume (%s)\n", volume_path);
	return -1;
    }

    (void)file_path;
    (void)file_stat;
    (void)ret;

    if (__fs_touch(vol, "foo", NEXUS_DIR)) {
	return -1;
    }

    if (__fs_touch(vol, "foo/bar.txt", NEXUS_REG)) {
	return -1;
    }

    if (__fs_touch(vol, "cow", NEXUS_DIR)) {
	return -1;
    }

    if (__fs_touch(vol, "cow/meat.txt", NEXUS_REG)) {
	return -1;
    }

    return 0;
}
