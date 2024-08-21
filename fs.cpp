#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include "fs.h"

void load_fat(uint16_t* FAT, uint8_t* block) {
    Disk disk;
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
}

void save_fat(uint16_t* FAT, uint8_t* block) {
    Disk disk;
    // Write FAT to disk
    for (int i = 0; i < sizeof(block)/2; i++) {
        block[i*2] = FAT[i] / 256;
        block[i*2+1] = FAT[i] % 256;
        
        // DEBUG
        // if (i < 10) {
        //     std::cout << i << ": " << FAT[i] << " -> " << FAT[i] / 256 << " | " << FAT[i] % 256 << std::endl;
        // }
    }

    // DEBUG
    // for (int i = 0; i < sizeof(block); i++) {
    //     if (i < 10) {
    //         std::cout << i << ": " << (int) block[i] << std::endl;
    //     }
    // }

    disk.write(1, block);
}

int free_block(int* block_no, uint16_t* FAT) {
    // Find free block by iterating over FAT
    bool found = false;
    for (int i = 2; i < sizeof(FAT)/2 && !found; i++) {
        if (i != *block_no && FAT[i] == 0) {
            *block_no = i;
            found = true;
        }
    }
    // Return -1 if no free block is found
    if (found = false) {
        return -1;
    }
    return 0;
}

void put_direntry(dir_entry* file, uint8_t* block, int ptr) {
    // std::cout << "PUT DIRENTRY --------------" << std::endl;
    // std::cout << "file: " << file->file_name << std::endl;
    // std::cout << "block: " << file->first_blk << std::endl;
    // std::cout << "size: " << file->size << std::endl;
    uint8_t* attributes = reinterpret_cast<uint8_t*>(file);
    for (int i = 0; i < sizeof(dir_entry); i++) {
        block[ptr+i] = attributes[i];
    }
    // std::cout << "---------------------------" << std::endl;
}

void get_direntry(dir_entry** file, uint8_t* block, int ptr) {
    std::cout << "GET DIRENTRY --------------" << std::endl;
    std::cout << "ptr: " << ptr << std::endl;
    uint8_t attributes[64] = {0};
    for (int i = 0; i < sizeof(dir_entry); i++) {
        attributes[i] = block[ptr+i];
    }
    dir_entry* entry = reinterpret_cast<dir_entry*>(attributes);
    *file = reinterpret_cast<dir_entry*>(attributes);
    std::cout << "file: " << entry->file_name << std::endl;
    std::cout << "block: " << entry->first_blk << std::endl;
    std::cout << "size: " << entry->size << std::endl;
    std::cout << "---------------------------" << std::endl;
}

void find_file(dir_entry** file, std::string filepath) {
    Disk disk;
    dir_entry* entry = NULL;
    *file = NULL;
    uint8_t block[4096] = {0};
    uint8_t attributes[64] = {0};

    disk.read(0, block);

    // Iterate over direntries
    for (int i = 0; i < sizeof(block) / sizeof(dir_entry); i++) {

        // Parse direntry
        get_direntry(&entry, block, i*sizeof(dir_entry));

        // Check if needle
        if (std::string(entry->file_name).compare(filepath) == 0) {
            *file = entry;
            return;
        }
    }
}

int find_free_space(int* ptr) {
    Disk disk;
    dir_entry* entry = NULL;
    uint8_t block[4096] = {0};
    uint8_t attributes[64] = {0};

    disk.read(0, block);

    // Iterate over direntries
    for (int i = 0; i < sizeof(block) / sizeof(dir_entry); i++) {

        // Parse direntry
        get_direntry(&entry, block, i*sizeof(dir_entry));

        // Check if file name empty
        if (entry->file_name[0] == 0) {
            *ptr = i;
            return 0;
        }
    }
    return -1;
}


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
    std::string filename = ".";
    filename.copy(file.file_name, sizeof(file.file_name));
    file.size = 64;
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
    dir_entry* f;
    uint16_t FAT[2048];
    
    uint8_t block[4096] = {0};
    int ptr = sizeof(file);
    int block_no;
    int next_block;
    
    int found; // Utilized in forwarding errors

    // Load FAT
    load_fat(FAT, block);

    // Find free block by iterating over FAT
    found = free_block(&block_no, FAT);

    // Return -1 if no free block is found
    if (found == -1) {
        return -1;
    }

    // Specify attributes
    filepath.copy(file.file_name, sizeof(file.file_name));
    file.first_blk = block_no;
    file.type = 0;
    file.access_rights = 0x06;

    // Read user data by line and track size to handle blocks.
    std::string line;
    std::string content;
    char c;
    bool eof = false;
    while (!eof) {
        std::getline(std::cin, line);
        std::stringstream linestream(line);
        if (!line.empty()) {
            while (linestream.get(c)) {
                content += (uint8_t)c;
            }
            // TODO: Add new-line per linestream?
        }
        else {
            eof = true;
        }
    }

    // Write file attributes to first block
    file.size = sizeof(content) + 64; // +64?
    // std::cout << "DBF1 --------------" << std::endl;
    put_direntry(&file, block, 0);
    // get_direntry(&f, block, 0);

    std::cout << "wrote att" << std::endl; 

    for (int i = 0; i < content.size(); i++) {
        block[ptr] = (uint8_t)content[i];
        ptr++;

        // Handle multiple blocks
        if (ptr > sizeof(block)) {

            // Write old block
            disk.write(block_no, block);

            // Find free block by iterating over FAT
            int found = free_block(&next_block, FAT);
            if (found == -1) {
                return -1;
            }

            // Update FAT
            FAT[block_no] = next_block;
            block_no = next_block;

            // Reset ptr and block
            ptr = 0;
            std::cout << "Ping" << std::endl;
            std::fill(std::begin(block), std::end(block), 0);
            std::cout << "Pong" << std::endl;
        }
    }

    // Write block to disk and update FAT
    disk.write(block_no, block);
    FAT[block_no] = (uint16_t) -1;

    // Read root
    disk.read(0, block);

    std::cout << "find space" << std::endl; 
    // Get free root spot
    found = find_free_space(&ptr);
    if (found == -1) {
        return -1;
    }

    std::cout << "enter dir into root" << std::endl; 
    // Enter direntry into root
    // std::cout << "DBF2 --------------" << std::endl;
    put_direntry(&file, block, ptr*sizeof(dir_entry));
    // get_direntry(&f, block, ptr*sizeof(dir_entry));

    std::cout << "save root" << std::endl; 
    // Save root
    disk.write(0, block);

    std::cout << "save fat" << std::endl; 
    // Write FAT to disk
    save_fat(FAT, block);

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    Disk disk;

    dir_entry* file;

    uint16_t FAT[2048];

    uint8_t block[4096];
    int ptr = sizeof(dir_entry);

    std::string text;

    // TODO: Check file existance

    // Load FAT
    load_fat(FAT, block);

    // TODO: Get block_no from file name
    int block_no = 2;
    int next_block;
    
    // Read file from disk
    disk.read(block_no, block);

    // Interpret block file attributes
    // uint8_t attributes[64];
    // for (int i = 0; i < sizeof(attributes); i++) {
    //     attributes[i] = block[i];
    // }
    // dir_entry* file = reinterpret_cast<dir_entry*>(attributes);
    get_direntry(&file, block, 0);

    // DEBUG
    std::cout << file->file_name << std::endl;

    // While FAT not EOL / Get next block from FAT
    while (FAT[block_no] != (uint16_t) -1 && FAT[block_no] != 0) {
        bool b = FAT[block_no] != (uint16_t)-1;
        std::cout << "block_no: " << block_no << std::endl;
        std::cout << "FAT: " << FAT[block_no] << std::endl;
        std::cout << "-1: " << (uint16_t) -1 << std::endl;
        std::cout << "bool: " << b << std::endl;
        // Read text from block
        for (;ptr < sizeof(block); ptr++) {
            text += (char)block[ptr];
        }

        // Read next block from FAT
        next_block = FAT[block_no];
        block_no = next_block;

        // Read block from disk
        disk.read(block_no, block);
    }

    for (;ptr < sizeof(block); ptr++) {
        text += (char)block[ptr];
    }

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

    dir_entry* file;
    uint8_t block[4096] = {0};
    uint8_t attributes[64] = {0};

    // Read root from disk
    disk.read(0, block);

    std::cout << "name\tsize" << std::endl;

    // Iterate over direntries
    for (int i = 0; i < sizeof(block) / sizeof(dir_entry); i++) {

        // Parse direntry
        get_direntry(&file, block, i*sizeof(dir_entry));

        // If file exist
        std::cout << file->file_name << "\t" << file->size << std::endl;
        if (file->file_name[0] != 0) {

        }

    }

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
