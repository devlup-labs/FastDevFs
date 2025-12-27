#include<fuse3/fuse.h>
#include<iostream>
using namespace std;

int get_attr(const char* path, struct stat* stbuf, struct fuse_file_info* fi){
    cout<<"getattr called for path: "<<path<<endl;
    // Implementation of getattr would go here
    return 0;
}
int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags){
    cout<<"readdir called for path: "<<path<<endl;
    // Implementation of readdir would go here
    return 0;
}
static struct fuse_operations fastdevfs_oper = {
    .getattr = get_attr,
    .readdir = readdir,
};
int main(int argc, char* argv[]){
    cout<<"Starting FastDevFs Daemon..."<<endl;
    // Initialization code for FastDevFs would go here

    return fuse_main(argc, argv, &fastdevfs_oper, NULL);
}