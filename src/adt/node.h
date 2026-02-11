#pragma once

#include <string>
#include <unordered_map>
#include <sys/stat.h>
#include <cstring>
#include "inode.h"

enum class NodeType {
    FILE,
    DIRECTORY,
    SYMLINK
};

struct Node {
    std::string name;
    NodeType type;

    inode_t inode;      // 🔥 identity (NEW)

    Node* parent;

    std::string data;   // file content

    // only valid if DIRECTORY
    std::unordered_map<std::string, Node*> children;

    Node(const std::string& name, NodeType type, Node* parent, inode_t inode)
        : name(name), type(type), parent(parent), inode(inode) {}
};
