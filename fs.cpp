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

void files_to_block(dir_entry *files, uint8_t *block) {
    memcpy(block, reinterpret_cast<uint8_t*>(files), BLOCK_SIZE);
}

void block_to_files(dir_entry *files, uint8_t *block) {
    memcpy(files, reinterpret_cast<dir_entry*>(block), BLOCK_SIZE);
}

void text_to_block(char *text, uint8_t *block) {
    memcpy(block, reinterpret_cast<uint8_t*>(text), BLOCK_SIZE);
}

void block_to_text(char *text, uint8_t *block) {
    memcpy(text, reinterpret_cast<char*>(block), BLOCK_SIZE);
}

bool is_duplicate_name(dir_entry *files, char *file_name) {
    // return whether name is duplicate or not
    return true;
}

int free_block(uint16_t *fat) {
    // return the first free fat block
    for (int i = 0; i < BLOCK_SIZE / 2; i++) {
        // found free block
        std::cout << "FS::free_block(" << i << " : " << fat[i] << ")\n";
        if (fat[i] == (uint16_t)0) {
            std::cout << "FS::free_block(found: " << i << ")\n";
            return i;
        }
    }
    std::cout << "FS::free_block(no memory found)\n";
    return BLOCK_SIZE;
}

int free_file(dir_entry *files) {
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        // found free block
        std::cout << "FS::free_file(" << i << " : " << files[i].file_name << ")\n";
        if (!(files[i].size > (uint32_t)0)) {
            std::cout << "FS::free_file(found: " << i << " : " << files[i].file_name << ")\n";
            return i;
        }
    }
    return BLOCK_SIZE / sizeof(dir_entry);
}

int seek_file(dir_entry *files, std::string filepath) {
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        if (strcmp(files[i].file_name, filepath.c_str()) == 0) {
            return i;
        }
    }
    return -1;
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
    dir_entry files[64];
    disk.read(0, block);
    block_to_files(files, block);

    // Create file
    dir_entry file;

    // strncpy with sizeof to limit string size
    strncpy(file.file_name, filepath.c_str(), sizeof(file.file_name));

    // Get next free block
    int blk = free_block(fat);
    file.first_blk = blk; // write to file header
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

    std::cout << "FS::create(" << content << ")\n";
    std::cout << "FS::create(" << s << ")\n";

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

        // write block to disk
        disk.write(blk, block);

        // if additional iterations
        if (i + 1 < no_blocks) {
            // slice string for next iteration
            content = content.substr(4096);

            // find next block for writing
            // update fat, blockidx and EOF
            // order important to create headless ghost files instead of endless files/loops
            int next_blk = free_block(fat);
            fat[next_blk] = (uint16_t)-1;
            fat[blk] = next_blk;
            blk = next_blk;
        }
    }

    file.type = (uint8_t)0; // Set type to file. [0=file, 1=directory]
    file.access_rights = (uint8_t)7; // read (0x04) + write (0x02) + execute (0x01)

    // Find space in directory
    int fileidx = free_file(files);

    // Save file
    files[fileidx] = file;
    files_to_block(files, block);
    disk.write(0, block);

    // Save fat
    fat_to_block(fat, block);
    disk.write(1, block);

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(1, block);
    block_to_fat(fat, block);

    // GET FILES
    dir_entry files[64];
    disk.read(0, block);
    block_to_files(files, block);

    // seek file
    int fileidx = seek_file(files, filepath);
    // handle missing file
    if (fileidx == -1) {
        std::cout << "FS::cat(err: file not found)\n";
        return -1;
    }
    dir_entry file = files[fileidx];

    // get first block
    uint16_t blk = file.first_blk;
    std::string content;

    // iterate over file blocks
    char text[4096];
    disk.read(blk, block);
    block_to_text(text, block);
    std::string s(text, BLOCK_SIZE);
    content = content + s;

    // iterate until EOF
    while(fat[blk] != (uint16_t)-1) {
        blk = fat[blk];
        disk.read(blk, block);
        block_to_text(text, block);
        std::string s(text, BLOCK_SIZE);
        content = content + s;
    }

    std::cout << content << "\n";

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
