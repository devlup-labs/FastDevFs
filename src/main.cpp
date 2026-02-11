#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h> 
#include "metadata/metadata.h"
#include "fuse/fs.h"
#include "adt/fs_tree.h"

int main(int argc, char* argv[])
{
    init_metadata(".fastdevfs.meta");  // ✅ FIRST
    init_fs_tree();                    // ✅ SECOND
    init_ops();                        // FUSE ops
    return fuse_main(argc, argv, &ops, nullptr);
}

