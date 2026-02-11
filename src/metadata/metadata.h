#pragma once
#include <cstdint>
#include "../adt/inode.h"

#define FS_MAGIC 0xFADFCAFE
#define FS_VERSION 1
#define MAX_INODES 10000

struct MetadataHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t next_inode;
};

struct Metadata {
    MetadataHeader* header;
    Inode* inode_table;
};

extern Metadata meta;

void init_metadata(const char* path);
