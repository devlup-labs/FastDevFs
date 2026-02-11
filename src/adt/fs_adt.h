#pragma once

#include <sys/stat.h>
#include <vector>
#include <string>

int adt_getattr(const char* path, struct stat* st);
int adt_readdir(const char* path, std::vector<std::string>& out);
int adt_mkdir(const char* path, mode_t mode);
int adt_mknod(const char* path, mode_t mode);
int adt_rename(const char* from, const char* to);
int adt_write(const char* path, const char* buf, size_t size, off_t offset);
int adt_read(const char* path, char* buf, size_t size, off_t offset);

