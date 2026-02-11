#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include "../adt/fs_adt.h"
#include <cstring>
#include <vector>
#include <string>
#include "fs.h"
static int fs_getattr(const char *p, struct stat *s, struct fuse_file_info *)
{
    memset(s, 0, sizeof(*s));
    return adt_getattr(p, s);
}

static int fs_readdir(const char *p, void *buf, fuse_fill_dir_t f,
                      off_t, struct fuse_file_info *, enum fuse_readdir_flags)
{
    std::vector<std::string> v;
    adt_readdir(p, v);
    f(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
    f(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
    for (auto &s : v)
        f(buf, s.c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
    return 0;
}

static int fs_mknod(const char *p, mode_t m, dev_t)
{
    return adt_mknod(p, m);
}

static int fs_mkdir(const char *p, mode_t m)
{
    return adt_mkdir(p, m);
}

static int fs_rename(const char *from, const char *to, unsigned int)
{
    return adt_rename(from, to);
}

static int fs_utimens(const char *path, const struct timespec tv[2],
                      struct fuse_file_info *)
{
    return 0; // ignore for now
}

static int fs_write(const char *path,
                    const char *buf,
                    size_t size,
                    off_t offset,
                    struct fuse_file_info *)
{
    return adt_write(path, buf, size, offset);
}

static int fs_read(const char *path,
                   char *buf,
                   size_t size,
                   off_t offset,
                   struct fuse_file_info *)
{
    return adt_read(path, buf, size, offset);
}

/* ---- GLOBAL ops (DEFINED HERE) ---- */
struct fuse_operations ops;

void init_ops()
{
    memset(&ops, 0, sizeof(ops));
    ops.getattr = fs_getattr;
    ops.readdir = fs_readdir;
    ops.mknod = fs_mknod;
    ops.mkdir = fs_mkdir;
    ops.rename = fs_rename;
    ops.utimens = fs_utimens;
    ops.write = fs_write;
    ops.read = fs_read;
}
