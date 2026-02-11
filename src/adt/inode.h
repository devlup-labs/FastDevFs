#pragma once
#include <sys/stat.h>
#include <cstdint>

using inode_t = uint64_t;

struct Inode {
    inode_t id;
    struct stat st;
};
