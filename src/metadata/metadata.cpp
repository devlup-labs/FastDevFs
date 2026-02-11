#include "metadata.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

Metadata meta; // ✅ MATCHES HEADER

void init_metadata(const char *path)
{
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
        throw std::runtime_error("open failed");

    size_t size =
        sizeof(MetadataHeader) +
        MAX_INODES * sizeof(Inode);

    if (ftruncate(fd, size) < 0)
        throw std::runtime_error("ftruncate failed");

    void *base = mmap(nullptr, size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);

    if (base == MAP_FAILED)
        throw std::runtime_error("mmap failed");

    meta.header = reinterpret_cast<MetadataHeader *>(base);
    meta.inode_table = reinterpret_cast<Inode *>(
        (char *)base + sizeof(MetadataHeader));

    if (meta.header->magic != FS_MAGIC)
    {
        memset(base, 0, size);
        meta.header->magic = FS_MAGIC;
        meta.header->version = FS_VERSION;
        meta.header->next_inode = 1;
    }

    close(fd);
}
