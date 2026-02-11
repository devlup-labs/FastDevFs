#include "fs_tree.h"
#include "inode_alloc.h"
#include "../metadata/metadata.h"

FileSystemTree *fs_tree = nullptr;

void init_fs_tree()
{
    fs_tree = new FileSystemTree();

    inode_t root_ino = alloc_inode(S_IFDIR | 0755);
    meta.inode_table[root_ino].st.st_nlink = 2;

    fs_tree->root = new Node("/", NodeType::DIRECTORY, nullptr, root_ino);
}
