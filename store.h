#ifndef STORE_H
#define STORE_H

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

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

struct store_super_block {
    uint32_t magic;           // Identity
    uint16_t version;         // Something relating to config stuff
    uint16_t flags;           // features enabled BITS

    uint64_t disk_size;       // total disk file size
    uint32_t block;           // block size
    uint32_t inode_start;     // block index
    uint32_t inode_end;       // block index

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

// config struct to populate from user [store.toml]
struct store_config {
    int         compression;    // is compression enabled for all of disk
    uint64_t    disk_size;      // size of disk file
    char        disk_name[256]; // name of disk file
    char        usage[16];      // to get inode ratio
};

#endif