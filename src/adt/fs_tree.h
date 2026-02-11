#pragma once
#include "node.h"

struct FileSystemTree {
    Node* root;
};

extern FileSystemTree* fs_tree;
void init_fs_tree();
