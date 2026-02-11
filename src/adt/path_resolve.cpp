#include "path_resolve.h"
#include "fs_tree.h"
#include <sstream>
#include <vector>

static std::vector<std::string> split(const char *path)
{
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;

    while (std::getline(ss, item, '/'))
    {
        if (!item.empty())
            parts.push_back(item);
    }
    return parts;
}

Node *resolve_path(const char *path)
{
    if (strcmp(path, "/") == 0)
        return fs_tree->root;

    Node *cur = fs_tree->root;
    auto parts = split(path);

    for (auto &p : parts)
    {
        if (cur->type != NodeType::DIRECTORY)
            return nullptr;

        if (!cur->children.count(p))
            return nullptr;

        cur = cur->children[p];
    }
    return cur;
}

Node *resolve_parent(const char *path, std::string &child)
{
    auto parts = split(path);
    if (parts.empty())
        return nullptr;

    child = parts.back();
    parts.pop_back();

    Node *cur = fs_tree->root;
    for (auto &p : parts)
    {
        if (!cur->children.count(p))
            return nullptr;
        cur = cur->children[p];
    }
    return cur;
}
