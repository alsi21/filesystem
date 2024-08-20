#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{

}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n";

    Disk disk;

    // Create root directory.
    uint8_t block[4096] = {0};

    dir_entry file;
    std::string filename = "root";
    filename.copy(file.file_name, sizeof(file.file_name));
    file.size = 0;
    file.first_blk = 0;
    file.type = 1;
    file.access_rights = 0x06;

    uint8_t* attributes = reinterpret_cast<uint8_t*>(&file);

    for (int i = 0; i < sizeof(file); i++) {
        block[i] = attributes[i];
    }

    // Write root to disk
    disk.write(0, block);

    // Init FAT and mark all as free by EOF.
    uint8_t FAT[4096] = {0};
    FAT[0] = 255;
    FAT[1] = 255;
    FAT[2] = 255;
    FAT[3] = 255;

    // Write FAT to disk.
    disk.write(1, FAT);

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";

    Disk disk;

    // TODO: Check file already exist.

    dir_entry file;
    unsigned short FAT[2048];
    
    uint8_t block[4096] = {0};
    int ptr = sizeof(file);
    int block_no;
    int next_block;
    
    // Load FAT
    disk.read(1, block);
    
    // DEBUG
    // for (int i = 0; i < sizeof(block); i++) {
    //     if (i < 10) {
    //         std::cout << i << ": " << (int) block[i] << std::endl;
    //     }
    // }

    for (int i = 0; i < sizeof(block)/2; i++) {
        
        // DEBUG
        // if (i < 10) {
        //     std::cout << i << ": " << (block[i*2] << 8 | block[i*2+1]) << " <- " << (int)block[i*2] << " | " << (int)block[i*2+1] << std::endl;
        // }

        FAT[i] = block[i*2] << 8 | block[i*2+1];
    }

    // Find free block by iterating over FAT
    bool found = false;
    for (int i = 2; i < sizeof(block)/2 && !found; i++) {
        if (i != block_no && FAT[i] == 0) {
            block_no = i;
            found = true;
        }
    }
    // Return -1 if no free block is found
    if (found = false) {
        return -1;
    }

    // Write attributes
    filepath.copy(file.file_name, sizeof(file.file_name));
    file.size = 64;
    file.first_blk = 2;
    file.type = 0;
    file.access_rights = 0x06;

    uint8_t* attributes = reinterpret_cast<uint8_t*>(&file);

    for (int i = 0; i < sizeof(file); i++) {
        block[i] = attributes[i];
    }

    // Read user data by line and track size to handle blocks.

    std::string line;
    char c;
    bool eof = false;
    while (!eof) {
        std::getline(std::cin, line);
        std::stringstream linestream(line);
        if (!line.empty()) {
            while (linestream.get(c)) {
                block[ptr] = (uint8_t)c;
                ptr++;
            }
            // TODO: Add new-line per linestream?
        }
        else {
            eof = true;
        }
        
        // Handle multiple blocks
        if (ptr > sizeof(block)) {

            // Write old block
            disk.write(block_no, block);
            FAT[block_no] = next_block;
            
            // Find free block by iterating over FAT
            found = false;
            for (int i = 2; i < sizeof(block)/2 && !found; i++) {
                if (i != block_no && FAT[i] == 0) {
                    next_block = i;
                    found = true;
                }
            }
            // Return -1 if no free block is found
            if (found = false) {
                return -1;
            }

            // Update FAT
            FAT[block_no] = next_block;
            block_no = next_block;

            // Write file attributes to block
            for (int i = 0; i < sizeof(file); i++) {
                block[i] = attributes[i];
            }

            // Reset ptr and continue
            ptr = sizeof(file);
        }

    }
    // Write block to disk and update FAT
    disk.write(block_no, block);
    FAT[block_no] = -1;

    // Write FAT to disk
    for (int i = 0; i < sizeof(block)/2; i++) {
        block[i*2] = FAT[i] / 255;
        block[i*2+1] = FAT[i] % 255;
        
        // DEBUG
        // if (i < 10) {
        //     std::cout << i << ": " << FAT[i] << " -> " << FAT[i] / 256 << " | " << FAT[i] % 256 << std::endl;
        // }
    }
    disk.write(1, block);

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    Disk disk;

    uint8_t attributes[64];

    uint8_t block[4096];
    int ptr = sizeof(dir_entry);

    std::string text;

    // TODO: Get block_no from file name
    int block_no;
    
    // Read file content
    disk.read(block_no, block);

    // Interpret attributes
    for (int i = 0; i < sizeof(attributes); i++) {
        attributes[i] = block[i];
    }
    dir_entry* file = reinterpret_cast<dir_entry*>(attributes);

    // DEBUG
    std::cout << file->file_name << std::endl;

    // TODO: While FAT not EOL / Get next block from FAT
        // Read text from block
        for (;ptr < sizeof(block); ptr++) {
            text += (char)block[ptr];
        }

        // Read next block from FAT

        // Check if EOL

        // Read block from disk

    // DEBUG
    std::cout << text << std::endl;

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";

    Disk disk;

    uint8_t block[4096];

    disk.read(0, block);

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
