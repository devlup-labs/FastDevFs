# FastDevFs — Demo Commands

## 1. Build

```bash
cd /home/mohit/Desktop/FastDevFs
# Create a separate build directory to keep source tree clean (out-of-source build)
mkdir -p build && cd build

# Generate Makefiles from CMakeLists.txt — detects compilers, finds FUSE3, downloads GoogleTest and CLI11
cmake ..

# Compile all targets in parallel using all CPU cores for speed
make -j$(nproc)

cd ..
```

## 2. Run Tests

```bash
cd build

# Run all 6 test suites (ADT, Hash, Persistence, NodeShare, Predictor, LibraryDetection)
# --verbose shows individual test case pass/fail output instead of just a summary
ctest --verbose

cd ..
```

---

## 3. CLI Tool (Offline — No Daemon Needed)

### Hash a file
```bash
# Compute SHA-256 hash of a file — same algorithm used internally for dedup content matching
./build/fastdevfs-cli hash /etc/hostname
```

### Scan a directory
```bash
# Recursively scan a directory to count files, directories, and total size
# Useful for seeing what would be deduplicated before mounting
./build/fastdevfs-cli scan /tmp --recursive
```

### Show config
```bash
# Display all current configuration values from config.ini
# Shows paths, dedup settings, log level, thread count, etc.
./build/fastdevfs-cli config get
```

### Set config value
```bash
# Enable deduplication (can also be set to false to disable)
./build/fastdevfs-cli config set dedup_enabled true

# Set the mount point path that the daemon will use
./build/fastdevfs-cli config set mountpoint /fastdevfsdemo
```

### Library whitelist management
```bash
# List all 595+ tracked libraries (react, lodash, express, webpack, etc.)
# These are recognized instantly via Tier 1 binary search during folder dedup
./build/fastdevfs-cli library list

# Check if a specific library name exists in the tracked whitelist
./build/fastdevfs-cli library check lodash

# Add a custom library name — it will be detected by Tier 1 binary search from now on
./build/fastdevfs-cli library add my-custom-lib

# Remove a library from the whitelist
./build/fastdevfs-cli library remove my-custom-lib
```

---

## 4. Start the Daemon

### Option A: Using CLI tool
```bash
# Start the daemon via CLI — handles cleanup, validation, and mounting automatically
./build/fastdevfs-cli start -m /fastdevfsdemo
```

### Option B: Direct (foreground, shows logs — recommended for demo)
```bash
# Clean all persistence files from previous runs to start fresh
# .mmap files = filesystem tree + dedup index, fastdevfs_data = actual file content store
rm -f /tmp/fastdevfs.mmap /tmp/fastdevfs_dedup.mmap /tmp/fastdevfs_lib_config.txt
rm -rf /tmp/fastdevfs_data

# Create mount point and data directory (FUSE requires an empty directory to mount on)
mkdir -p /fastdevfsdemo /tmp/fastdevfs_data

# Start the FUSE daemon in foreground mode (-f) so all logs are visible in this terminal
# Loads 595 tracked libraries + MLP predictor model, starts dedup worker thread
./build/fastdevfs /fastdevfsdemo -f
```

> Open a **second terminal** for all the commands below.

---

## 5. File Deduplication Demo

```bash
# Create the first file — dedup worker hashes it after 500ms debounce and registers it as canonical
echo "hello world" > /fastdevfsdemo/file1.txt

# Wait for debounce timer + hashing to complete
sleep 3

# Create a second file with identical content — dedup worker detects matching SHA-256 hash
# and hardlinks it to file1's data, saving disk space
echo "hello world" > /fastdevfsdemo/file2.txt

# Wait for dedup processing
sleep 3

# Verify deduplication: both data files should share the SAME inode number with nlink=2
# nlink=2 means two directory entries point to the same physical data (hardlinked)
ls -li /tmp/fastdevfs_data/
```

**Expected output:** Both data files share the same inode number with nlink=2.

**Daemon log shows:**
```
[Dedup] Registered canonical /tmp/fastdevfs_data/1 hash=a948904f...
[Dedup] Linked /tmp/fastdevfs_data/2 → /tmp/fastdevfs_data/1 (refcount=2)
```

---

## 6. Copy-on-Write (CoW) Break Demo

```bash
# Modify one of the deduplicated files — triggers CoW break
# The system detects file2 shares data with file1 (nlink > 1), reads the original content,
# creates a new independent copy for file2, then writes the new content
echo "modified content" > /fastdevfsdemo/file2.txt

# Wait for processing
sleep 2

# Verify CoW break: files now have DIFFERENT inodes with nlink=1 each
# file1 keeps original "hello world", file2 has "modified content" — fully independent
ls -li /tmp/fastdevfs_data/
```

**Expected output:** Two different inodes, each with nlink=1.

**Daemon log shows:**
```
[Dedup] CoW break for index=2
[Dedup] Registered canonical /tmp/fastdevfs_data/2 hash=52b32727...
```

---

## 7. Library/Folder Deduplication Demo

```bash
# Create project A with a lodash library inside node_modules
# Each mkdir is separate because FUSE handles one directory creation at a time
mkdir /fastdevfsdemo/projectA
mkdir /fastdevfsdemo/projectA/node_modules
mkdir /fastdevfsdemo/projectA/node_modules/lodash

# Add files to the library — these represent a typical npm package
echo 'module.exports = {}' > /fastdevfsdemo/projectA/node_modules/lodash/index.js
echo '{"name":"lodash","version":"4.17.21"}' > /fastdevfsdemo/projectA/node_modules/lodash/package.json

# Wait 5s for the 3-second settlement timer to fire + folder signature computation
# The system: detects "lodash" via Tier 1 binary search → computes recursive folder signature
# → registers it as the canonical copy in the library catalog
sleep 5

# Create project B with an IDENTICAL lodash library (simulates npm install in another project)
mkdir /fastdevfsdemo/projectB
mkdir /fastdevfsdemo/projectB/node_modules
mkdir /fastdevfsdemo/projectB/node_modules/lodash
echo 'module.exports = {}' > /fastdevfsdemo/projectB/node_modules/lodash/index.js
echo '{"name":"lodash","version":"4.17.21"}' > /fastdevfsdemo/projectB/node_modules/lodash/package.json

# Wait for settlement — the system computes projectB/lodash's folder signature,
# finds it matches projectA/lodash's canonical signature, and node-shares the entire subtree
# Both directories now share the same internal tree nodes (zero extra storage)
sleep 5

# Verify: both lodash folders show identical content with same timestamps
# The files are shared at the tree level — not just hardlinked, but the entire subtree is shared
ls -la /fastdevfsdemo/projectA/node_modules/lodash/
ls -la /fastdevfsdemo/projectB/node_modules/lodash/
```

**Daemon log shows:**
```
[LibraryDedup] Registered new canonical library folder: /projectA/node_modules/lodash
[LibraryDedup] Duplicate library detected — node-sharing: /projectB/node_modules/lodash → canonical
[NodeShare] Linked /projectB/node_modules/lodash → canonical
```

---

## 8. CLI Tool (Online — Daemon Running)

```bash
# Query the running daemon for live status — shows PID, mountpoint, dedup state, log level
./build/fastdevfs-cli status

# Trigger a full deduplication sweep across all files in the mounted filesystem
# Displays a live progress bar with percentage, then shows summary (files processed, duplicates, space saved)
./build/fastdevfs-cli dedup run

# Show current dedup statistics — total duplicates found, space saved, per-file details with refcounts
./build/fastdevfs-cli dedup stats
```

---

## 9. Stop the Daemon

### Option A: CLI
```bash
# Gracefully stop the daemon — sends IPC shutdown command, unmounts FUSE, cleans up PID/socket files
./build/fastdevfs-cli stop
```

### Option B: Manual
```bash
# Unmount the FUSE filesystem directly using the FUSE utility
fusermount -u /fastdevfsdemo

# Or press Ctrl+C in the daemon terminal if running in foreground mode
```

---

## Key Talking Points

| Feature | Mechanism | Timing |
|---------|-----------|--------|
| File dedup | SHA-256 content hash → hardlink identical files | 500ms debounce |
| CoW break | Detects shared file on write → creates independent copy | Instant on write |
| Library detection | Tier 1: binary search in 595+ known libs, Tier 2: MLP neural network | On folder creation |
| Folder dedup | Recursive folder signature → node-sharing of entire subtree | 3s settlement timer |
| Persistence | mmap-backed tree + dedup index survive restarts | Always on |
