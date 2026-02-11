#include "blob_store.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static const char *ROOT = "/tmp/fastdevfs_blobs";

std::string blob_path(const std::string &h)
{
    return std::string(ROOT) + "/" + h.substr(0, 2) + "/" + h;
}

int blob_write(const std::string &h, int fd)
{
    std::string path = blob_path(h);
    mkdir((std::string(ROOT) + "/" + h.substr(0, 2)).c_str(), 0755);

    int out = open(path.c_str(), O_CREAT | O_WRONLY, 0644);
    if (out < 0)
        return -1;

    char buf[8192];
    lseek(fd, 0, SEEK_SET);
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(out, buf, n);

    close(out);
    return 0;
}

int blob_read(const std::string &h, char *buf, size_t size, off_t off)
{
    int fd = open(blob_path(h).c_str(), O_RDONLY);
    if (fd < 0)
        return -1;
    lseek(fd, off, SEEK_SET);
    int r = read(fd, buf, size);
    close(fd);
    return r;
}
