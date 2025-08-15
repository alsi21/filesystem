#include <iostream>
#include <string>
#include <string.h>
#include "fs.h"
#include "disk.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{

}

void fat_to_block(uint16_t *fat, uint8_t *block) {
    // converts block of uint16 fat entries to block of uint8 to enable usage of disk interface
    for (int i = 0; i < BLOCK_SIZE / 2; i++) {
        block[i * 2] = (uint8_t)(fat[i] >> 8); // Might need readjustment
        block[i * 2 + 1] = (uint8_t)fat[i];
    }
}

void block_to_fat(uint16_t *fat, uint8_t *block) {
    // converts block of uint8, provided by the disk, to block of uint16 fat entries
    for (int i = 0; i < BLOCK_SIZE / 2; i++) {
        fat[i] = (uint16_t)(block[i * 2] << 8) + block[i * 2 + 1]; // Might need readjustment
    }
}

void text_to_block(char *text, uint8_t *block) {
    block = reinterpret_cast<uint8_t*>(text);
}

bool is_duplicate_name(dir_entry *files, char *file_name) {
    // return whether name is duplicate or not
    return true;
}

int free_block(uint16_t *fat) {
    // return the first free fat block
    for (int i = 0; i < BLOCK_SIZE / 2; i++) {
        // found free block
        if (fat[i] = (uint16_t)0) return i;
    }
    return 1;
}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n";
    
    // Init root
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.write(0, block);
    
    // Init FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    fat[0] = (uint16_t)-1; // root
    fat[1] = (uint16_t)-1; // FAT
    fat_to_block(fat, block);
    disk.write(1, block);
    
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";
    
    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(1, block);
    block_to_fat(fat, block);

    // GET FILES
    // PRESUMABLY array of 64 disk_entries
    dir_entry files[64]; // TODO

    // Create file
    dir_entry file;

    // strncpy with sizeof to limit string size
    strncpy(file.file_name, filepath.c_str(), sizeof(file.file_name));

    // Get next free block
    file.first_blk = free_block(fat);
    int blk = file.first_blk; // keep track of current block
    fat[blk] = (uint16_t)-1; // set EOF

    // Get content of file
    std::string content;
    std::string s;
    do {
        std::getline(std::cin, s);
        // TODO write as we go?
        content = content + s;
    }
    while (!s.empty());

    // Save size
    file.size = (uint32_t)sizeof(content.c_str());

    // Convert string to blocks
    // TODO: handle empty files
    // Iterate over blocks needed to store content
    int no_blocks = (int)((content.length() + 4095) / 4096);
    for (int i = 0; i < no_blocks; i++) {

        // convert string slice to block
        char text[4096];
        strncpy(text, content.c_str(), sizeof(text));
        text_to_block(text, block);
        disk.write(blk, block);

        // if additional iterations
        if (i + 1 < no_blocks) {
            // slice string for next iteration
            content = content.substr(4096);

            // find next block for writing
            int next_blk = free_block(fat);
            // update fat, blockidx and EOF
            fat[blk] = next_blk;
            blk = next_blk;
            fat[blk] = (uint16_t)-1;
        }
    }


    file.type = (uint8_t)0;
    file.access_rights = (uint8_t)7; // read (0x04) + write (0x02) + execute (0x01)

    // Save files
    files[0] = file;
    block = reinterpret_cast<uint8_t*>(&files);
    disk.write(0, block);
    
    // Save fat

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
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
