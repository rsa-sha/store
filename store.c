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
    printf("Config:\n");
    printf("\t config.compression - %d\n", config.compression);
    printf("\t config.disk_size - %ld\n", config.disk_size);
    printf("\t config.disk_name - %s\n", config.disk_name);
    printf("\t config.usage - %s\n", config.usage);
    if (config.disk_size == 0) {
        printf("Disk size cannot be zero\n");
        return -1;
    }
    return 0;
}


int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (init_config(argc, argv)!= 0) {
        exit(EXIT_FAILURE);
    }
}