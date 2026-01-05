#include "store.h"

struct store_config config;

void usage(char *val) {
    printf("Usage: %s\n", val);
    printf("Flags: \n");
    printf("\t -co|--config config_file [Fetches from cur file or goes default mode]\n");
    printf("\t -cm|--compression on|off [Default is off] \n");
    printf("\t -dn|--disk_name [Default -> store.disk\n");
    printf("\t -ds|--disk_size bytes|VALUE[KB|MB|GB] [ERR if neither in config, nor passed during init] \n");
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
    printf("Config:\n");
    printf("\t%-20s - %-10d\n", "config.compression", config.compression);
    printf("\t%-20s - %-10ld\n", "config.disk_size", config.disk_size);
    printf("\t%-20s - %-10s\n", "config.disk_name", config.disk_name);
    printf("\t%-20s - %-10s\n", "config.usage", config.usage);
    printf("\t%-20s - %-10d\n", "config.block_size", config.block_size);
    printf("\t%-20s - %-10d\n", "config.ratio", config.ratio);
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
    uint32_t n_inodes = (inode_space/sizeof(struct store_inode));
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


// 1->YES; 0->NO
int search(int argc, char **argv, const char* key) {
    for(int i = 0; i < argc; i++) {
        if (strcasecmp(argv[i], key)== 0)
            return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    // this should only happen when the user does `store init` or `store rewrite`
    if (init_config(argc, argv)!= 0) {
        exit(EXIT_FAILURE);
    }
    // init process flow
    if (search(argc, argv, "init")==1) {
        printf("init\n");
        // the user wants to init the disk
        if (command_init()!=0) {
            printf("Ran into errors while initiating disk!");
            exit(EXIT_FAILURE);
        }
        printf("Disk %s initiated, can be used now\n", config.disk_name);
        exit(EXIT_SUCCESS);
    }
}