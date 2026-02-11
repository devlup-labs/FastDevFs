#include "fs_adt.h"
#include "path_resolve.h"
#include "inode_alloc.h"
#include "../metadata/metadata.h"

#include <errno.h>
#include <cstring>
#include <algorithm>

/* ---------- getattr ---------- */
int adt_getattr(const char *path, struct stat *st)
{
    Node *n = resolve_path(path);
    if (!n)
        return -ENOENT;

    *st = meta.inode_table[n->inode].st;
    return 0;
}

/* ---------- readdir ---------- */
int adt_readdir(const char *path, std::vector<std::string> &out)
{
    Node *dir = resolve_path(path);
    if (!dir || dir->type != NodeType::DIRECTORY)
        return -ENOTDIR;

    for (auto &[name, _] : dir->children)
        out.push_back(name);

    return 0;
}

/* ---------- mkdir ---------- */
int adt_mkdir(const char *path, mode_t mode)
{
    std::string name;
    Node *parent = resolve_parent(path, name);
    if (!parent)
        return -ENOENT;

    if (parent->children.count(name))
        return -EEXIST;

    inode_t ino = alloc_inode(S_IFDIR | mode);
    meta.inode_table[ino].st.st_nlink = 2;

    Node *d = new Node(name, NodeType::DIRECTORY, parent, ino);
    parent->children[name] = d;

    meta.inode_table[parent->inode].st.st_nlink++;
    return 0;
}

/* ---------- mknod ---------- */
int adt_mknod(const char *path, mode_t mode)
{
    std::string name;
    Node *parent = resolve_parent(path, name);
    if (!parent)
        return -ENOENT;

    if (parent->children.count(name))
        return -EEXIST;

    inode_t ino = alloc_inode(S_IFREG | mode);
    Node *f = new Node(name, NodeType::FILE, parent, ino);

    parent->children[name] = f;
    return 0;
}

/* ---------- rename ---------- */
int adt_rename(const char *from, const char *to)
{
    std::string name_from, name_to;

    Node *parent_from = resolve_parent(from, name_from);
    if (!parent_from)
        return -ENOENT;

    if (!parent_from->children.count(name_from))
        return -ENOENT;

    Node *node = parent_from->children[name_from];

    Node *parent_to = resolve_parent(to, name_to);
    if (!parent_to)
        return -ENOENT;

    if (parent_to->children.count(name_to))
        return -EEXIST;

    parent_from->children.erase(name_from);

    node->name = name_to;
    node->parent = parent_to;
    parent_to->children[name_to] = node;

    return 0;
}

/* ---------- write ---------- */
int adt_write(const char* path, const char* buf, size_t size, off_t offset)
{
    Node* n = resolve_path(path);
    if (!n) return -ENOENT;
    if (n->type != NodeType::FILE) return -EISDIR;

    std::string &data = n->data;

    if (offset + size > data.size())
        data.resize(offset + size);

    memcpy(&data[offset], buf, size);

    meta.inode_table[n->inode].st.st_size = data.size();
    return size;
}

/* ---------- read ---------- */
int adt_read(const char* path, char* buf, size_t size, off_t offset)
{
    Node* n = resolve_path(path);
    if (!n) return -ENOENT;
    if (n->type != NodeType::FILE) return -EISDIR;

    std::string &data = n->data;

    if (offset >= data.size())
        return 0;

    size_t to_read = std::min(size, data.size() - offset);
    memcpy(buf, data.data() + offset, to_read);

    return to_read;
}
