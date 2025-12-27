# Running Guide (for developers)
*We will keep updating this file with more detailed instructions.*
## Install the FastDevFS
To install FastDevFS, follow these steps:
1. Clone the repository:
   ```bash
   git clone https://github.com/devlup-labs/FastDevFs.git
   cd FastDevFs
   ```
2. Ensure you have the necessary dependencies installed, including FUSE3 and a C++ compiler.
    ``` bash
    sudo apt-get install libfuse3-dev g++
    ```
3. Run the following command in sudo mode
    ``` bash
    sudo su
    g++ main.cpp -Wall `pkg-config fuse3 --cflags --libs` -o fastdevfs
    mkdir test_mount_dir
    ./fastdevfs test_mount_dir -f
    ```
