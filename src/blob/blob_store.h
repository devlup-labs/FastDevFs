#pragma once
#include <string>

std::string blob_path(const std::string& hash);
int blob_write(const std::string& hash, int fd);
int blob_read(const std::string& hash, char* buf, size_t size, off_t off);
