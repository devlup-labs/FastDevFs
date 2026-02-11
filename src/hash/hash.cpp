#include "hash.h"
#include <openssl/sha.h>

std::string sha256_fd(int fd)
{
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    char buf[8192];
    lseek(fd, 0, SEEK_SET);

    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        SHA256_Update(&ctx, buf, n);

    unsigned char hash[32];
    SHA256_Final(hash, &ctx);

    static const char *hex = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 32; i++)
    {
        out.push_back(hex[hash[i] >> 4]);
        out.push_back(hex[hash[i] & 0xf]);
    }
    return out;
}
