#pragma once

#include <fuse3/fuse.h>

/* global FUSE operations table */
extern struct fuse_operations ops;

/* initializes ops */
void init_ops();
