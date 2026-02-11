cmake_minimum_required(VERSION 3.10)
project(fastdevfs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED fuse3)

add_executable(fastdevfs
    src/main.cpp
    src/fuse/fs.cpp
    src/adt/fs_adt.cpp
    src/adt/fs_tree.cpp
    src/adt/path_resolve.cpp
    src/adt/inode_alloc.cpp
    src/metadata/metadata.cpp
)



target_include_directories(fastdevfs PRIVATE
    ${FUSE_INCLUDE_DIRS}
    src
)


target_link_libraries(fastdevfs
    ${FUSE_LIBRARIES}
)
