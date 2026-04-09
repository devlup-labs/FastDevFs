#include "../include/library_dedup.h"
#include "../include/dedup_index.h"
#include "../include/sha256.h"
#include "../include/library_predictor.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mutex>

// ============================================================
// Library catalog globals (canonical folder hash → index)
// ============================================================

std::unordered_map<std::string, int> g_lib_catalog;
std::mutex catalog_mtx;
std::string g_config_path;

// ============================================================
// Two-tier detection globals
// ============================================================

static std::vector<std::string> g_tracked_libraries;    // Sorted vector for binary search
static std::string g_tracked_json_path;                  // Path to tracked_libraries.json
static fastdevfs::LibraryPredictor g_predictor;          // MLP model (Tier 2)
static std::mutex g_tracked_mtx;                         // Protects vector + file writes

// ============================================================
// Extern declarations
// ============================================================

extern DedupIndex g_index;
struct treefile;
extern treefile* file1;

// ============================================================
// Tracked libraries initialization (Tier 1 setup)
// ============================================================

void init_tracked_libraries(const std::string& json_path) {
    std::lock_guard<std::mutex> lock(g_tracked_mtx);
    g_tracked_json_path = json_path;
    g_tracked_libraries.clear();

    std::ifstream infile(json_path);
    if (!infile.is_open()) {
        std::cerr << "[LibraryDedup] Could not open tracked_libraries.json: "
                  << json_path << std::endl;
        return;
    }

    // Simple JSON array parser: extract all quoted strings inside the array
    std::string content((std::istreambuf_iterator<char>(infile)),
                         std::istreambuf_iterator<char>());
    infile.close();

    // Find the array bounds
    size_t arr_start = content.find('[');
    size_t arr_end = content.rfind(']');
    if (arr_start == std::string::npos || arr_end == std::string::npos) {
        std::cerr << "[LibraryDedup] Invalid JSON format in " << json_path << std::endl;
        return;
    }

    // Extract strings between quotes
    size_t pos = arr_start;
    while (pos < arr_end) {
        size_t q1 = content.find('"', pos);
        if (q1 == std::string::npos || q1 >= arr_end) break;
        size_t q2 = content.find('"', q1 + 1);
        if (q2 == std::string::npos || q2 >= arr_end) break;

        std::string lib_name = content.substr(q1 + 1, q2 - q1 - 1);
        if (!lib_name.empty()) {
            g_tracked_libraries.push_back(lib_name);
        }
        pos = q2 + 1;
    }

    // Sort for binary search
    std::sort(g_tracked_libraries.begin(), g_tracked_libraries.end());

    std::cout << "[LibraryDedup] Loaded " << g_tracked_libraries.size()
              << " tracked libraries from " << json_path << std::endl;
}

// ============================================================
// Predictor initialization (Tier 2 setup)
// ============================================================

void init_predictor(const std::string& model_path) {
    if (g_predictor.load(model_path)) {
        std::cout << "[LibraryDedup] MLP predictor loaded from " << model_path << std::endl;
    } else {
        std::cerr << "[LibraryDedup] Failed to load MLP predictor from " << model_path
                  << " — Tier 2 detection disabled" << std::endl;
    }
}

// ============================================================
// Helper: extract library name from a folder path
// ============================================================

static std::string extract_library_name(const std::string& folder_path) {
    // Normalize: strip trailing slashes
    std::string path = folder_path;
    while (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    if (path.empty()) return "";

    // Find last slash to get the last segment
    size_t last_slash = path.rfind('/');
    std::string last_segment;
    if (last_slash == std::string::npos) {
        last_segment = path;
    } else {
        last_segment = path.substr(last_slash + 1);
    }

    // Handle scoped npm packages: if last segment starts with @, we need @scope/name
    // But actually the @ is on the *parent* directory, e.g., /node_modules/@babel/core
    // So check if the segment before last_segment starts with @
    if (last_slash != std::string::npos && last_slash > 0) {
        size_t prev_slash = path.rfind('/', last_slash - 1);
        std::string parent_segment;
        if (prev_slash == std::string::npos) {
            parent_segment = path.substr(0, last_slash);
        } else {
            parent_segment = path.substr(prev_slash + 1, last_slash - prev_slash - 1);
        }

        if (!parent_segment.empty() && parent_segment[0] == '@') {
            // Scoped package: return "@scope/name"
            return parent_segment + "/" + last_segment;
        }
    }

    return last_segment;
}

// ============================================================
// Helper: append a new library name to tracked_libraries.json
// ============================================================

static void append_to_tracked_json(const std::string& folder_name) {
    if (g_tracked_json_path.empty()) return;

    // Read existing content
    std::ifstream infile(g_tracked_json_path);
    if (!infile.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(infile)),
                         std::istreambuf_iterator<char>());
    infile.close();

    // Find the closing bracket of the array
    size_t close_bracket = content.rfind(']');
    if (close_bracket == std::string::npos) return;

    // Find the last non-whitespace character before ]
    size_t last_content = content.find_last_not_of(" \t\n\r", close_bracket - 1);

    // Build the insertion: add comma if there are existing entries
    std::string insertion;
    if (last_content != std::string::npos && content[last_content] != '[') {
        insertion = ",\n    \"" + folder_name + "\"";
    } else {
        insertion = "\n    \"" + folder_name + "\"";
    }

    // Insert before the closing bracket
    content.insert(close_bracket, insertion);

    // Write back
    std::ofstream outfile(g_tracked_json_path);
    if (outfile.is_open()) {
        outfile << content;
        outfile.close();
    }
}

// ============================================================
// Two-tier library detection
// ============================================================

bool check_model_is_library(const std::string& folder_path) {
    std::string lib_name = extract_library_name(folder_path);
    if (lib_name.empty()) return false;

    // --- Tier 1: Binary search in sorted tracked libraries vector ---
    {
        std::lock_guard<std::mutex> lock(g_tracked_mtx);
        if (std::binary_search(g_tracked_libraries.begin(),
                               g_tracked_libraries.end(),
                               lib_name)) {
            return true;
        }
    }

    // --- Tier 2: MLP predictor fallback ---
    if (g_predictor.is_loaded()) {
        if (g_predictor.is_library(folder_path)) {
            // Model says it's a library — add to tracked set for future fast lookups
            std::lock_guard<std::mutex> lock(g_tracked_mtx);

            // Insert maintaining sorted order
            auto it = std::lower_bound(g_tracked_libraries.begin(),
                                       g_tracked_libraries.end(),
                                       lib_name);
            if (it == g_tracked_libraries.end() || *it != lib_name) {
                g_tracked_libraries.insert(it, lib_name);
                append_to_tracked_json(lib_name);
                std::cout << "[LibraryDedup] Model detected new library: \""
                          << lib_name << "\" — added to tracked_libraries.json"
                          << std::endl;
            }
            return true;
        }
        return false;
    }

    // --- Fallback: old heuristic (if predictor failed to load) ---
    if (folder_path.find("/node_modules") != std::string::npos ||
        folder_path.find("/vendor") != std::string::npos) {
        return true;
    }
    return false;
}

// ============================================================
// Library catalog (canonical folder hash → tree index)
// ============================================================

void init_library_catalog(const std::string& config_file_path) {
    std::lock_guard<std::mutex> lock(catalog_mtx);
    g_config_path = config_file_path;
    std::ifstream infile(config_file_path);
    if (!infile.is_open()) return;

    std::string line;
    while (std::getline(infile, line)) {
        size_t comma = line.find(',');
        if (comma != std::string::npos) {
            std::string hash = line.substr(0, comma);
            int canonical_idx = std::stoi(line.substr(comma + 1));
            g_lib_catalog[hash] = canonical_idx;
        }
    }
}

static void save_catalog() {
    std::ofstream outfile(g_config_path);
    for (const auto& pair : g_lib_catalog) {
        outfile << pair.first << "," << pair.second << "\n";
    }
}

// ============================================================
// Folder dedup internals
// ============================================================

static void get_all_files_in_folder(int folder_idx, std::vector<int>& files, treefile& tf) {
    int child = tf.arr[folder_idx].firstchild;
    while(child != -1 && child < tf.size) {
        treenode& node = tf.arr[child];
        if (!node.isdeleted) {
            if (S_ISDIR(node.metadata.mode)) {
                get_all_files_in_folder(child, files, tf);
            } else if (S_ISREG(node.metadata.mode)) {
                files.push_back(child);
            }
        }
        child = node.nextsibling;
    }
}

std::string compute_folder_signature(int folder_idx) {
    std::vector<int> files;
    std::string folder_path;
    {
        std::lock_guard<std::recursive_mutex> lk(treefile_mtx);
        if (!file1) return "";
        folder_path = file1->arr[folder_idx].metadata.name;
        get_all_files_in_folder(folder_idx, files, *file1);
    }

    std::vector<std::string> file_sig_parts;
    for (int file_idx : files) {
        long size = 0;
        std::string rel_path;
        {
            std::lock_guard<std::recursive_mutex> lk(treefile_mtx);
            size = file1->arr[file_idx].metadata.size;
            std::string full_path = file1->arr[file_idx].metadata.name;
            if (full_path.rfind(folder_path, 0) == 0) {
                rel_path = full_path.substr(folder_path.length());
            } else {
                rel_path = full_path;
            }
        }
        std::string file_hash = g_index.get_inode_hash(file_idx);

        std::ostringstream ss;
        ss << rel_path << "|" << size << "|" << file_hash;
        file_sig_parts.push_back(ss.str());
    }

    // Empty folders must not produce a valid signature — they would all
    // collide on the SHA-256 of an empty string and get node-shared.
    if (file_sig_parts.empty()) return "";

    std::sort(file_sig_parts.begin(), file_sig_parts.end());

    std::ostringstream master;
    for(const auto& s : file_sig_parts) {
        master << s << "\n";
    }
    return sha256_hex(master.str().c_str(), master.str().length());
}

static std::string host_data_path_for(int index) {
    return "/tmp/fastdevfs_data/" + std::to_string(index);
}

static void do_node_share(int target_folder_idx, int canonical_folder_idx) {
    std::lock_guard<std::recursive_mutex> lk(treefile_mtx);
    if (!file1) return;
    dedup_link(target_folder_idx, canonical_folder_idx, *file1);
}

// Check if folder_path is inside a known dependency container directory.
// Returns true for paths like "/projectA/node_modules/lodash" but NOT
// for "/projectA" or "/projectA/node_modules" itself.
static bool is_inside_dependency_container(const std::string& path) {
    // Known dependency container directory names
    static const char* containers[] = {
        "/node_modules/", "/vendor/", "/.venv/", "/venv/",
        "/__pycache__/", "/bower_components/", "/.gradle/",
        "/target/debug/", "/target/release/", "/site-packages/",
        "/gems/", "/pkg/", "/Pods/",
        nullptr
    };
    for (int i = 0; containers[i]; i++) {
        if (path.find(containers[i]) != std::string::npos) return true;
    }
    return false;
}

void evaluate_and_deduplicate_library_folder(int folder_idx) {
    if (!file1) return;

    std::string folder_path;
    {
        std::lock_guard<std::recursive_mutex> lk(treefile_mtx);
        if (folder_idx < 0 || folder_idx >= file1->size) return;
        treenode& node = file1->arr[folder_idx];
        if (node.isdeleted || !S_ISDIR(node.metadata.mode)) return;
        folder_path = node.metadata.name;
    }

    // Only evaluate folders that are inside a known dependency container
    // (e.g., /proj/node_modules/lodash). Skip arbitrary user directories
    // like /projectA or even the container itself (/proj/node_modules).
    if (!is_inside_dependency_container(folder_path)) return;

    if (!check_model_is_library(folder_path)) return;

    std::string sig = compute_folder_signature(folder_idx);
    if (sig.empty()) return;

    std::lock_guard<std::mutex> lock(catalog_mtx);

    // Guard: never node-share a folder that is an ancestor (or descendant)
    // of an existing canonical.  This prevents e.g. /proj/node_modules being
    // linked to its own child /proj/node_modules/lodash.
    for (const auto& [cat_sig, cat_idx] : g_lib_catalog) {
        std::string cat_path;
        {
            std::lock_guard<std::recursive_mutex> lk(treefile_mtx);
            cat_path = file1->arr[cat_idx].metadata.name;
        }
        if (folder_path.rfind(cat_path, 0) == 0 && folder_path != cat_path)
            return;  // folder_path is a descendant of an existing canonical
        if (cat_path.rfind(folder_path, 0) == 0 && cat_path != folder_path)
            return;  // folder_path is an ancestor of an existing canonical
    }

    auto it = g_lib_catalog.find(sig);
    if (it != g_lib_catalog.end()) {
        int canonical_folder_idx = it->second;
        if (canonical_folder_idx != folder_idx) {
            std::cout << "[LibraryDedup] Duplicate library detected — node-sharing: "
                      << folder_path << " → canonical idx " << canonical_folder_idx
                      << " (" << file1->arr[canonical_folder_idx].metadata.name << ")\n";
            do_node_share(folder_idx, canonical_folder_idx);
        }
    } else {
        std::cout << "[LibraryDedup] Registered new canonical library folder: " << folder_path
                  << " (Signature: " << sig.substr(0, 8) << "...)" << std::endl;
        g_lib_catalog[sig] = folder_idx;
        save_catalog();
    }
}
