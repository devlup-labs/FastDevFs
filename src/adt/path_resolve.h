#pragma once
#include "node.h"

Node* resolve_path(const char* path);
Node* resolve_parent(const char* path, std::string& child_name);
