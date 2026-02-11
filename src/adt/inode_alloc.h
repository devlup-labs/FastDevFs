#pragma once
#include <sys/stat.h>
#include "inode.h"

inode_t alloc_inode(mode_t mode);
