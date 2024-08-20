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

    // Init FAT and mark all as free by EOF.
    uint8_t FAT[4096] = {0}; // TODO: EOF?

    // Write FAT to disk.
    Disk disk;
    unsigned int block_no = 1;
    disk.write(block_no, FAT);

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";

    Disk disk;

    dir_entry file;
    uint8_t block[4096] = {0};
    int ptr = 0;

    // Write attributes

    filepath.copy(file.file_name, sizeof(file.file_name));
    file.size = 64;
    file.first_blk = 2;
    file.type = 0;
    file.access_rights = 0x06;

    uint8_t* attributes = reinterpret_cast<uint8_t*>(&file);

    for (;ptr < sizeof(file); ptr++) {
        block[ptr] = attributes[ptr];
    }

    // Read user data by line and track size to handle blocks.

    std::string line;
    char c;
    bool terminate = false;
    while (!terminate) {
        std::getline(std::cin, line);
        std::stringstream linestream(line);
        if (!line.empty()) {
            while (linestream.get(c)) {
                block[ptr] = (uint8_t)c;
                ptr++;
            }
        }
        else {
            terminate = true;
        }
    }

    //uint8_t* data = reinterpret_cast<uint8_t*>(&block);

    disk.write(2, block);

    // Check file already exist.

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    Disk disk;

    uint8_t block[4096];
    uint8_t attributes[64];
    int ptr = 0;

    std::string text;

    // Read file content

    disk.read(2, block);

    for (;ptr < sizeof(attributes); ptr++) {
        attributes[ptr] = block[ptr];
    }

    dir_entry* file = reinterpret_cast<dir_entry*>(attributes);

    std::cout << file->file_name << std::endl;

    for (;ptr < sizeof(block); ptr++) {
        text += (char)block[ptr];
    }

    std::cout << text << std::endl;

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";
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
