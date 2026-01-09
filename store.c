#include "store.h"

struct store_config config;

// Helper for SB issues
void print_super_block(const struct store_super_block *sb) {
    if (!sb) return;

    printf("=== Superblock Info ===\n");
    printf("Magic:           0x%08" PRIx32 "\n", sb->magic);
    printf("Version:         %" PRIu32 "\n", sb->version);
    printf("Flags:           0x%08" PRIx32 "\n", sb->flags);
    printf("Disk size:       %" PRIu64 " bytes\n", sb->disk_size);
    printf("Block size:      %" PRIu32 " bytes\n", sb->block);
    printf("Inode start:     %" PRIu32 "\n", sb->inode_start);
    printf("Inode end:       %" PRIu32 "\n", sb->inode_end);
    printf("Data start:      %" PRIu32 "\n", sb->data_start);
    printf("Data space left: %" PRIu64 " bytes\n", sb->data_space_left);
    printf("Compression:     %u\n", sb->compression);
    printf("Checksum:        %u\n", sb->checksum);
    printf("SB Checksum:     %zu\n", sb->sb_cksum);
    printf("Reserved:        %" PRIi32 "\n", sb->reserved);
    printf("=======================\n");
}

void usage(char *val) {
    printf("Usage: %s\n", val);
    printf("Flags: \n");
    printf("\t -co|--config config_file [Fetches from cur file or goes default mode]\n");
    printf("\t -cm|--compression on|off [Default is off] \n");
    printf("\t -ds|--disk [Default -> store.disk\n");
    printf("\t -dz|--disk_size bytes|VALUE[KB|MB|GB] [ERR if neither in config, nor passed during init] \n");
}

int read_config(char *file) {
    FILE *f = fopen(file, "r");
    if (f== NULL) {
        perror("Failed to open config_file");
        return 1;
    }
    fclose(f);
    // read and build line the config line by line [toml]
    return 0;
}


uint64_t parse_size(const char *s) {
    // TODO: Only takes int val if end alpha val is not in the list [MB|KB|GB]
    // if I do -ds 1G => size will be 1 byte instead of 1GB or whatever intended
    char *end;
    uint64_t val = strtoull(s, &end, 10);

    if (strcasecmp(end, "KB") == 0) return val * 1024;
    if (strcasecmp(end, "MB") == 0) return val * 1024 * 1024;
    if (strcasecmp(end, "GB") == 0) return val * 1024 * 1024 * 1024;

    return val;
}


int fill_config(char *file) {
    FILE *f = fopen(file, "r");
    if (f== NULL) {
        perror("Failed to open config_file");
        return 1;
    }
    char err_buf[4096];
    toml_table_t *conf = toml_parse_file(f, err_buf, 4096);
    fclose(f);
    if (!conf) {
        printf("Err parsing config, trying with defaults\nErr: %s\n", err_buf);
        return 1;
    }
    // reading values of fields
    toml_table_t *disk = toml_table_in(conf, "disk");
    if (disk) {
        toml_datum_t name = toml_string_in(disk, "name");
        if (!name.ok) {
            printf("Can't read name of disk in config correctly");
        } else {
            strcpy(config.disk_name, name.u.s);
        }
        toml_datum_t size = toml_string_in(disk, "size");
        if (!size.ok) {
            printf("Can't read size of disk in config correctly");
        } else if (config.disk_size==0) {
            config.disk_size = parse_size(size.u.s);
        }
    }

    // storage profile enums [COMPRESSION and other options]
    toml_table_t *storage = toml_table_in(conf, "storage");
    if (storage) {
        toml_datum_t profile = toml_string_in(storage, "profile");
        if (!profile.ok) {
            printf("Can't read storage profile config correctly");
            strcpy(config.usage,"balanced");
        } else {
            strcpy(config.usage, profile.u.s);
        }
    }
    toml_table_t *layout = toml_table_in(conf, "layout");
    if (layout) {
        toml_datum_t bs = toml_string_in(layout, "block_size");
        if (bs.ok)
            config.block_size = parse_size(bs.u.s);
    }
    return 0;
}


void print_config() {
    printf("Config:\n");
    printf("\t%-20s - %-10d\n", "config.compression", config.compression);
    printf("\t%-20s - %-10ld\n", "config.disk_size", config.disk_size);
    printf("\t%-20s - %-10s\n", "config.disk_name", config.disk_name);
    printf("\t%-20s - %-10s\n", "config.usage", config.usage);
    printf("\t%-20s - %-10d\n", "config.block_size", config.block_size);
    printf("\t%-20s - %-10d\n", "config.ratio", config.ratio);
}

int init_config(int argc, char **argv) {
    for(int i = 1; i< argc; i++) {
        if ((strcmp(argv[i], "-co")==0 || strcmp(argv[i], "--config")==0) && i+1 < argc) {
            // build from config file
            if(read_config(argv[i+1]) != 0) {
                return -1;
                // failure in reading & building from config
            } else {
                // get params build superblock, fill inodes
                if (fill_config(argv[i+1])!=0) {
                    // invalid config going default;
                    config.compression = 0;     // no compression
                    //config.disk_size = 0;       // has to check user param now if remains 0 will raise config err
                    strcpy(config.usage, "BALANCED");
                }
            }
        } else if ((strcmp(argv[i], "-ds")==0 || strcmp(argv[i], "--disk_size")==0) && i+1 < argc) {
            config.disk_size = parse_size(argv[i+1]);
        } else if ((strcmp(argv[i], "-dn")==0 || strcmp(argv[i], "--disk_name")==0) && i+1 < argc) {
                strcpy(config.disk_name, argv[i+1]);
        }
    }
    // to be used for No. of inodes calculation
    if (strcasecmp(config.usage, "largefile")) {
        config.ratio = LARGE;
    } else if (strcasecmp(config.usage, "smallfiles")) {
        config.ratio = SMALL;
    } else {
        config.ratio = BALANCED;
    }
    if (config.disk_size == 0) {
        printf("Disk size cannot be zero\n");
        return -1;
    }
    return 0;
}

// Assumes that config is there [init_config], populates SUPERBLOCK & INODES for use
int command_init() {
    // only runs when user does `store init ...`
    // based on the conf creates disk file in exec-dir and inits it
    // creating the file
    int fd = open(config.disk_name, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        perror("Unable to create disk file\n");
        return 1;
    }
    // allocating size to disk file

    if (ftruncate(fd, config.disk_size)!=0){
        perror("ftruncate err\n");
        return 1;
    }

    /*
    config.ratio specifies the inode ratio as a constant (n_inodes).
    This ratio is divided by 1000 to get the percentage of disk size used for inodes.
    Example:
        ratio       [SMALL]         -> 40
        multiplier  [ratio / 1000]  -> 0.04
        disk_size   [4096MB]        -> 4294967296   [4096 * 1024 * 1024]
        ----------------------------------------------------------------
        To calculate available space for inodes, we subtract the size of the superblock:
        Available Space = disk_size - sizeof(struct store_super_block)
        Let's assume sizeof(store_super_block) = 4KB -> 4096 bytes
            Available Space = 4294967296 - 4096 -> 4294963200 bytes
        Space allocated for inodes = Available Space * multiplier
            Space for inodes = 4294963200 * 0.04 -> 171798528 bytes (~163 MB)
        We round down this number to the nearest multiple of sizeof(store_inode) to get
        the final amount of space allocated for inodes.

    Note:
        - The size of `store_super_block` and `store_inode` are system-dependent
          (typically defined as structures in the file system).
    */

    uint64_t disk_avail = config.disk_size - sizeof(struct store_super_block);
    uint64_t inode_space = (disk_avail * config.ratio)/1000;
    uint64_t n_inodes = (inode_space/sizeof(struct store_inode));
    uint64_t inode_space_used = n_inodes * sizeof(struct store_inode);

    // super_block
    struct store_super_block sb = {0};
    sb.magic = STORE_MAGIC;
    sb.version = 0;                     // first config
    sb.flags = 0;                       // no features enabled
    sb.disk_size = config.disk_size;
    sb.block = config.block_size ? config.block_size:4096;
    sb.inode_start = 1;                 // 0 reserver for SB
    sb.inode_end = sb.inode_start + n_inodes - 1;
    sb.data_start = sb.inode_end + 1;
    sb.data_space_left = disk_avail - inode_space_used;
    sb.compression = config.compression;
    sb.checksum = 0;                    // unused for now
    sb.sb_cksum = 0;                    // unused for now
    sb.reserved = 0;                    // unused for now

    // Set compression bit in flag if enabled disk-wide
    if (sb.compression != 0)
        sb.flags = sb.flags | FLAG_COMPRESSION;

    // padded superblock
    char *sb_block = malloc(sb.block);
    if (sb_block==NULL) {
        printf("Unable to alloc mem to block");
        close(fd);
        return 1;
    }
    memset(sb_block, 0, sb.block);
    memcpy(sb_block, &sb, sizeof(sb));
    // writing superblock
    if (pwrite(fd, sb_block, 4096, 0) != 4096) {
        printf("Error writing super-block to disk");
        close(fd);
        return 1;
    }
    free(sb_block);

    // print_super_block(&sb);
    struct store_inode inode = {0};
    // inode inherits flags and compression from sb
    inode.flags = sb.flags;
    inode.compression = sb.compression;

    off_t inode_off = (off_t)sb.inode_start * config.block_size;

    for (uint32_t i = 0; i < n_inodes; i++) {
        if (pwrite(fd, &inode, sizeof(inode),
                inode_off + i * sizeof(inode)) != sizeof(inode)) {
            perror("write inode");
            close(fd);
            return 1;
        }
    }


    return 0;
}


/*
Search term args -> Returns idx of key in argv, else -1
Usage => search(n_args, arr_args, "key", ignore_case_or_not_flag)
*/
int search(int argc, char **argv, const char* key, bool ignore_case) {
    for(int i = 0; i < argc; i++) {
        if (ignore_case && strcasecmp(argv[i], key)== 0)
            return i;
        else if (strcmp(argv[i],key) == 0)
            return i;
    }
    return -1;
}

/*
This method looks for disk when config doesn't have it
If disk is found, we return 0, else ERR_VAL
*/
int look_for_disk(int argc, char **argv) {
    char cwd[PATH_MAX] = {0};
    int got_cwd = 0;
    if (getcwd(cwd, sizeof(cwd))==NULL) {
        got_cwd = -1;
        // return -1;      // could not find cwd
    }
    // first check if disk is passed in args
    int idx = -1;
    if ((idx = search(argc, argv, "--disk", false)) != -1) {
        if (idx + 1 >= argc){
            // there is no disk passed next to disk flag
            usage(argv[0]);
            printf("Disk name not passed after --disk flag\n");
            return -1;
        }
        if (argv[idx+1][0]=='/') {
            // absolute path given
            strcpy(config.disk_name, argv[idx+1]);
        } else {
            if (got_cwd == -1)
                return -1;
            char fname[PATH_MAX*2];
            snprintf(fname, PATH_MAX*2, "%s/%s", cwd, argv[idx+1]);
            strcpy(config.disk_name, fname);
        }
    } else if ((idx = search(argc, argv, "-ds", false))!= -1) {
        if (idx + 1 >= argc){
            // there is no disk passed next to disk flag
            usage(argv[0]);
            printf("Disk name not passed after -ds flag\n");
            return -1;
        }
        if (argv[idx+1][0]=='/') {
            // absolute path given
            strcpy(config.disk_name, argv[idx+1]);
        } else {
            if (got_cwd == -1)
                return -1;
            char fname[PATH_MAX*2];
            snprintf(fname, PATH_MAX*2, "%s/%s", cwd, argv[idx+1]);
            strcpy(config.disk_name, fname);
        }
    }
    return 0;
}


/*
Called from "write"|"append" after fetching disk name
Checks if disk has been "init" as a failsafe to not write somewhere else
Checks file access, file mode, sb.magic,sb.disk_size
Returns 0 if disk verified else ERR_VAL
*/
int verify_disk() {
    int fd = open(config.disk_name, O_RDONLY);      // DO NOT create if not exists
    if (fd < 0) {
        printf("Unable to open disk file -> %s\n", config.disk_name);
        return 1;
    }

    // stat disk info
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Unable to get disk info\n");
        close(fd);
        return -1;
    }

    // will be of use later on
    // if (!S_ISREG(st.st_mode)) {
    //     fprintf(stderr, "disk is not a regular file\n");
    //     close(fd);
    //     return -1;
    // }

    mode_t mode = st.st_mode & 0777;
    if (mode != 0644) {
        printf("Mode of disk file is different from the one at init\n");
        close(fd);
        return 1;
        // TODO: will add different file modes later
    }

    struct store_super_block sb;
    ssize_t n = pread(fd, &sb, sizeof(sb), 0);

    if (n != sizeof(sb)) {
        printf("Failed to read sb of disk\n");
        close(fd);
        return 1;
    }

    if (sb.magic !=STORE_MAGIC) {
        printf("Invalid store magic (bad disk)\n");
        close(fd);
        return 2;
    }

    if (sb.disk_size != (uint64_t) st.st_size) {
        printf("Disk size mismatch\n");
        close(fd);
        return 3;
    }
    config.disk_size = sb.disk_size;
    close(fd);
    return 0;

}


uint64_t verify_data(int argc, char **argv, enum data_in *write_source) {
    // 3 ways to send data for "write"/"append"
    int i = -1;
    uint64_t data = 0;
    if ((i=search(argc, argv, "--file",false))>0 && (i+1 < argc)) {
        // will check if file is given and it exists
        struct stat st;
        if (stat(argv[i+1], &st) != 0) {
            printf("Unable to access file %s given by user\n", argv[i+1]);
        } else {
            *write_source = D_FIL;
            data = st.st_size;
            return data;
        }
    } else if ((i=search(argc, argv, "--data",false))>0 && (i+1 < argc)) {
        *write_source = D_STR;
        data = strlen(argv[i+1]);
        return data;
    }
    // default is stdin but will do it later
    return data;
}


/*
Called after verify_data to check available space
-1 is not enough space else 0
*/
int check_available_space(uint64_t write_sz_b) {
    int fd = open(config.disk_name, O_RDONLY);      // DO NOT create if not exists
    if (fd < 0) {
        printf("Unable to open disk file -> %s\n", config.disk_name);
        return -1;
    }
    struct store_super_block sb;
    ssize_t n = pread(fd, &sb, sizeof(sb), 0);
    // print_super_block(&sb);

    if (n != sizeof(sb)) {
        printf("Failed to read sb of disk\n");
        close(fd);
        return -1;
    }

    if (write_sz_b > sb.data_space_left) {
        return -1;
    } return 0;
}


int main(int argc, char **argv) {
    // for(int i=0; i<argc;i++){
    //     printf("%s ", argv[i]);
    // }
    // goto ret;
    config.populated = false;
    if (argc < 2) {
        usage(argv[0]);
        goto ret_failure;
    }
    // this should only happen when the user does `store init` or `store rewrite`
    if (search(argc, argv, "init",true)>0 || search(argc, argv, "rewrite",true)>0 ) {
        if (init_config(argc, argv)!= 0) {
            // thinking of not exiting
            // exit(RET_FAILURE);
            config.populated = false;
        } else {
            config.populated = true;
        }
    }
    // init process flow
    if (search(argc, argv, "init", true) > 0) {
        printf("init\n");
        // the user wants to init the disk
        if (command_init()!=0) {
            printf("Ran into errors while initiating disk!");
            goto ret_failure;
        }
        if (config.populated)
            print_config();
        printf("Disk %s initiated, can be used now\n", config.disk_name);
        goto ret;
    } else if (search(argc, argv, "write", true) > 0) {
        /*
        Usage of write command
        [] => Need at least one of these args
        [options] => [-co config_path | -ds disk_name | --disk disk_name]
        store write [options] [--file path | --data string]

        Data source (exactly one):
        --file <path>     Read data from file
        --data <string>   Use literal string
        (none)            Read from stdin
        */
        if (!config.populated ){//|| !config.disk_name) {
            // the user hasn't passed in config with cmd we have to check for disk
            if (look_for_disk(argc, argv) != 0) {
                // could not find disk
                printf("Unable to lookup disk for write\n");
                goto ret_failure;
            }
        }
        if (verify_disk() == 0) {
                printf("Disk file -> %s\n", config.disk_name);
                printf("Disk size -> %ld\n", config.disk_size);
        } else {
            goto ret;
        }
        // can write to disk, now we need to verify data, is it a file or string??
        uint64_t data_in_bytes = 0;
        enum data_in write_source = D_INV;
        data_in_bytes = verify_data(argc, argv, &write_source);
        if (data_in_bytes==0 || write_source == D_INV) {
            printf("Nothing to write\n");
            goto ret_failure;
        } else {
            printf("Data to be written -> %zu bytes\n", data_in_bytes);
        }
        // data verified, check data size with available data space on disk
        if (check_available_space(data_in_bytes) == -1) {
            printf("Data to be written is more than the space available\n");
            goto ret_failure;
        } else {
            printf("Writing data to disk\n");
            // write call
            goto ret;
        }
    }
ret:
    return EXIT_SUCCESS;
ret_failure:
    return EXIT_FAILURE;
}