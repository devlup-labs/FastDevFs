#include "../metadata/metadata.h"
#include <cstring>

inode_t alloc_inode(mode_t mode)
{
    inode_t id = meta.header->next_inode++;

    Inode& ino = meta.inode_table[id];
    ino.id = id;
    memset(&ino.st, 0, sizeof(ino.st));
    ino.st.st_mode = mode;
    ino.st.st_nlink = 1;

    return id;
}
