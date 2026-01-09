#ifndef STORE_H
#define STORE_H

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <sys/stat.h>
// #include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "tomlc99/toml.h"

/*
[ block 0 ]
+------------------+
| superblock       |
+------------------+

[ block 1..N ]
+------------------+
| inode table      |
+------------------+

[ block M..end ]
+------------------+
| data blocks      |
+------------------+
*/
#define STORE_MAGIC 0x53544F52  // 'STOR'
// feature flag bits
#define FLAG_COMPRESSION  (1 << 0)  // LSB of the flag var
#define FLAG_READ_ONLY    (1 << 1)  // This file is read only for cur user
#define FLAG_HIDDEN       (1 << 2)  // This file is hidden, won't show up in cmds by default
#define FLAG_ENCRYPTED    (1 << 3)  // Needs key to read/write to this file
#define FLAG_DIRECTORY    (1 << 4)  // Is a dir

struct store_super_block {
    uint32_t magic;           // Identity
    uint16_t version;         // Something relating to config stuff
    uint16_t flags;           // features enabled BITS
    // [ON superblock only compression and Enc can be enabled disk-wide]

    uint64_t disk_size;       // total disk file size
    uint32_t block;           // block size
    uint32_t inode_start;     // block index
    uint32_t inode_end;       // block index
    uint64_t data_space_left; // available space for Writing

    uint32_t data_start;      // block index
    uint8_t compression;      // ENUM compression
    uint8_t checksum;         // ENUM cksum
    uint16_t reserved;
    uint64_t sb_cksum;        // Integrity

};

#define INODE_NAME 64           // Including /0
// Inode struct
struct store_inode {
    uint32_t inode_id;
    uint32_t flags;

    char name[INODE_NAME];
    uint64_t size;           // logical size
    uint64_t start_block;    // where data begins
    uint64_t block_count;    // how many blocks

    uint8_t  compression;    // inherited or overridden
    uint8_t  reserved[7];

    uint64_t checksum;

};

// divide by 1000 for use [percentages]
enum inode_ratio {
    LARGE=1,
    BALANCED=10,
    SMALL=40,
};

// Data source enum to know where's data is coming from
enum data_in {
    D_INV = 0,
    D_STR = 1,
    D_FIL = 2,
} data_in;

// config struct to populate from user [store.toml]
struct store_config {
    int                 compression;    // is compression enabled for all of disk
    uint64_t            disk_size;      // size of disk file
    char                disk_name[256]; // name of disk file
    char                usage[16];      // to get inode ratio
    int                 block_size;     // to be given in SB, if not 4KB by default
    enum inode_ratio    ratio;          // computed from inode_ratio and config.usage
    bool                populated;      // True if config read else False
};


// Other Macros
#define PATH_MAX 512


#endif