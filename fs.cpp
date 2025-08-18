#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <string.h>
#include "fs.h"
#include "disk.h"

#define DEBUG false

FS::FS()
{
    if (DEBUG) std::cout << "FS::FS()... Creating file system\n";
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

bool is_duplicate_name(dir_entry *files, std::string filepath) {
    // iterate over files
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        // check name
        if (DEBUG) std::cout << "FS::is_duplicate_name(" << i << " : " << files[i].file_name << ")\n";
        if (strcmp(files[i].file_name, filepath.c_str()) == 0) {
            if (DEBUG) std::cout << "FS::is_duplicate_name(duplicate of: " << i << " : " << files[i].file_name << ")\n";
            return true;
        }
    }
    return false;
}

int free_block(uint16_t *fat) {
    // return the first free fat block
    for (int i = 0; i < BLOCK_SIZE / 2; i++) {
        // found free block
        if (DEBUG) std::cout << "FS::free_block(" << i << " : " << fat[i] << ")\n";
        if (fat[i] == (uint16_t)0) {
            if (DEBUG) std::cout << "FS::free_block(found: " << i << ")\n";
            return i;
        }
    }
    if (DEBUG) std::cout << "FS::free_block(no memory found)\n";
    return -1; // TODO: catch error
}

int free_file(dir_entry *files) {
    // iterate over files
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        // check empty
        if (DEBUG) std::cout << "FS::free_file(" << i << " : " << files[i].file_name << ")\n";
        if (!(files[i].size > (uint32_t)0)) {
            // found free directory position
            if (DEBUG) std::cout << "FS::free_file(found: " << i << " : " << files[i].file_name << ")\n";
            return i;
        }
    }
    return -1; // TODO: catch this error
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
    if (DEBUG) std::cout << "FS::format()\n";
    
    // Init root
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.write(ROOT_BLOCK, block);
    
    // Init FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    fat[0] = (uint16_t)-1; // root
    fat[1] = (uint16_t)-1; // FAT
    fat_to_block(fat, block);
    disk.write(FAT_BLOCK, block);
    
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    if (DEBUG) std::cout << "FS::create(" << filepath << ")\n";
    
    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(FAT_BLOCK, block);
    block_to_fat(fat, block);

    // GET FILES
    dir_entry files[64];
    disk.read(ROOT_BLOCK, block);
    block_to_files(files, block);

    // Find space for file in directory
    int fileidx = free_file(files);

    if (fileidx == -1) {
        std::cout << "FS::create(err: directory full)\n";
        return -1;
    }

    if (filepath.length() > 55) {
        std::cout << "FS::create(err: filename \"" << filepath << "\" too long)\n";
        return -1;
    }

    if (is_duplicate_name(files, filepath)) {
        std::cout << "FS::create(err: file \"" << filepath << "\" already exists)\n";
        return -1;
    }

    // Create file
    dir_entry file;

    // strncpy with sizeof to limit string size
    strncpy(file.file_name, filepath.c_str(), 55); // allow names of length 55

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

    if (DEBUG) std::cout << "FS::create(" << content << ")\n";
    if (DEBUG) std::cout << "FS::create(" << s << ")\n";

    // Save size
    file.size = (uint32_t)content.size();

    if (file.size == 0) {
        std::cout << "FS::create(err: empty files are not allowed)\n";
        return -1;
    }

    // Convert string to blocks
    // Iterate over blocks needed to store content
    int no_blocks = (int)((file.size / BLOCK_SIZE) + 1);
    for (int i = 0; i < no_blocks; i++) {

        // convert string slice to block
        char text[BLOCK_SIZE];
        strncpy(text, content.c_str(), BLOCK_SIZE);
        text_to_block(text, block);

        // write block to disk
        disk.write(blk, block);

        // if additional iterations
        if (i + 1 < no_blocks) {
            // slice string for next iteration
            content = content.substr(BLOCK_SIZE - 1);

            // find next block for writing
            // update fat, blockidx and EOF
            int next_blk = free_block(fat);
            fat[next_blk] = (uint16_t)-1;
            fat[blk] = next_blk;
            blk = next_blk;
        }
    }

    file.type = (uint8_t)0; // Set type to file. [0=file, 1=directory]
    file.access_rights = (uint8_t)(READ + WRITE + EXECUTE); // read (0x04) + write (0x02) + execute (0x01)

    // Save file
    files[fileidx] = file;
    files_to_block(files, block);
    disk.write(ROOT_BLOCK, block);

    // Save fat
    fat_to_block(fat, block);
    disk.write(FAT_BLOCK, block);

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    if (DEBUG) std::cout << "FS::cat(" << filepath << ")\n";

    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(FAT_BLOCK, block);
    block_to_fat(fat, block);

    // GET FILES
    dir_entry files[64];
    disk.read(ROOT_BLOCK, block);
    block_to_files(files, block);

    // seek file
    int fileidx = seek_file(files, filepath);
    // handle missing file
    if (fileidx == -1) {
        std::cout << "FS::cat(err: file \"" << filepath << "\" not found)\n";
        return -1;
    }
    dir_entry file = files[fileidx];

    // get first block
    uint16_t blk = file.first_blk;
    std::string content;

    // iterate over file blocks
    char text[BLOCK_SIZE];
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
    if (DEBUG) std::cout << "FS::ls()\n";
    
    // GET FILES
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    dir_entry files[64];
    disk.read(ROOT_BLOCK, block);
    block_to_files(files, block);

    std::cout << std::left
        << std::setw(58) << "name"
        << std::setw(12) << "size"
        << "\n";
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        // found file
        if ((files[i].size > (uint32_t)0)) {
            dir_entry file = files[i];
            std::cout << std::left
                << std::setw(58) << file.file_name
                << std::setw(12) << file.size
                << "\n";
        }
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    if (DEBUG) std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";

    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(FAT_BLOCK, block);
    block_to_fat(fat, block);

    // GET FILES
    dir_entry files[64];
    disk.read(ROOT_BLOCK, block);
    block_to_files(files, block);

    // seek source file
    int sourceidx = seek_file(files, sourcepath);
    // handle missing source file
    if (sourceidx == -1) {
        std::cout << "FS::cp(err: file \"" << sourcepath << "\" not found)\n";
        return -1;
    }
    dir_entry sourcefile = files[sourceidx];

    // Find space for file in directory
    int destidx = free_file(files);

    if (destidx == -1) {
        std::cout << "FS::cp(err: directory full)\n";
        return -1;
    }

    // should not be possible
    if (sourcepath.length() > 55) {
        std::cout << "FS::cp(err: file \"" << sourcepath << "\" too long)\n";
        return -1;
    }
    if (destpath.length() > 55) {
        std::cout << "FS::cp(err: file \"" << destpath << "\" too long)\n";
        return -1;
    }

    if (is_duplicate_name(files, destpath)) {
        std::cout << "FS::cp(err: file \"" << destpath << "\" already exists)\n";
        return -1;
    }

    // get content
    std::string content;
    {
        // get first block
        uint16_t blk = sourcefile.first_blk;

        // iterate over file blocks
        char text[BLOCK_SIZE];
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
    }

    // write content to new file
    dir_entry destfile;
    {
        // strncpy with sizeof to limit string size
        strncpy(destfile.file_name, destpath.c_str(), 55); // allow names of length 55

        // Get next free block
        int blk = free_block(fat);
        destfile.first_blk = blk; // write to file header
        fat[blk] = (uint16_t)-1; // set EOF

        // Save size
        destfile.size = sourcefile.size;

        // should not be possible
        // if (destfile.size == 0) {
        //     std::cout << "FS::cp(err: empty files not allowed)\n";
        //     return -1;
        // }

        // Convert string to blocks
        // Iterate over blocks needed to store content
        int no_blocks = (int)((destfile.size / BLOCK_SIZE) + 1);
        for (int i = 0; i < no_blocks; i++) {

            // convert string slice to block
            char text[BLOCK_SIZE];
            strncpy(text, content.c_str(), BLOCK_SIZE - 1);
            text_to_block(text, block);

            // write block to disk
            disk.write(blk, block);

            // if additional iterations
            if (i + 1 < no_blocks) {
                // slice string for next iteration
                content = content.substr(BLOCK_SIZE - 1);

                // find next block for writing
                // update fat, blockidx and EOF
                int next_blk = free_block(fat);
                fat[next_blk] = (uint16_t)-1;
                fat[blk] = next_blk;
                blk = next_blk;
            }
        }

        destfile.type = (uint8_t)0; // Set type to file. [0=file, 1=directory]
        destfile.access_rights = (uint8_t)(READ + WRITE + EXECUTE); // read (0x04) + write (0x02) + execute (0x01)

        // Save file
        files[destidx] = destfile;
        files_to_block(files, block);
        disk.write(ROOT_BLOCK, block);

        // Save fat
        fat_to_block(fat, block);
        disk.write(FAT_BLOCK, block);
    }


    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    if (DEBUG) std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(FAT_BLOCK, block);
    block_to_fat(fat, block);

    // GET FILES
    dir_entry files[64];
    disk.read(ROOT_BLOCK, block);
    block_to_files(files, block);

    // seek source file
    int sourceidx = seek_file(files, sourcepath);
    // handle missing source file
    if (sourceidx == -1) {
        std::cout << "FS::mv(err: file \"" << sourcepath << "\" not found)\n";
        return -1;
    }
    dir_entry sourcefile = files[sourceidx];

    // should not be possible
    if (sourcepath.length() > 55) {
        std::cout << "FS::mv(err: file \"" << sourcepath << "\" too long)\n";
        return -1;
    }
    if (destpath.length() > 55) {
        std::cout << "FS::mv(err: file \"" << destpath << "\" too long)\n";
        return -1;
    }

    // handle duplicate name
    if (is_duplicate_name(files, destpath)) {
        std::cout << "FS::mv(err: file \"" << destpath << "\" already exists)\n";
        return -1;
    }

    if (true) {
        // handle rename

        // strncpy with sizeof to limit string size
        strncpy(sourcefile.file_name, destpath.c_str(), 55); // allow names of length 55

        // Save file
        files[sourceidx] = sourcefile;
        files_to_block(files, block);
        disk.write(ROOT_BLOCK, block);

    } else {
        // handle directory change
        
        // Find space for file in directory
        int destidx = free_file(files);

        // handle full directory
        if (destidx == -1) {
            std::cout << "FS::mv(err: directory full)\n";
            return -1;
        }

    }

    // Save fat
    fat_to_block(fat, block);
    disk.write(FAT_BLOCK, block);

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    if (DEBUG) std::cout << "FS::rm(" << filepath << ")\n";

    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(FAT_BLOCK, block);
    block_to_fat(fat, block);

    // GET FILES
    dir_entry files[64];
    disk.read(ROOT_BLOCK, block);
    block_to_files(files, block);

    // seek source file
    int fileidx = seek_file(files, filepath);
    // handle missing source file
    if (fileidx == -1) {
        std::cout << "FS::mv(err: file \"" << filepath << "\" not found)\n";
        return -1;
    }
    dir_entry file = files[fileidx];
    
    // save first block
    uint16_t blk = file.first_blk;

    // remove directory entry
    {
        files[fileidx] = dir_entry{};
    }
    
    // clear fat
    {
        // clear fat blocks until EOF
        while (fat[blk] != (uint16_t)-1) {
            uint16_t next_blk = fat[blk];
            fat[blk] = 0;
            blk = next_blk;
        }
        fat[blk] = 0;
    }

    // Save file
    files_to_block(files, block);
    disk.write(ROOT_BLOCK, block);

    // Save fat
    fat_to_block(fat, block);
    disk.write(FAT_BLOCK, block);

    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    if (DEBUG) std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";

    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(FAT_BLOCK, block);
    block_to_fat(fat, block);

    // GET FILES
    dir_entry files[64];
    disk.read(ROOT_BLOCK, block);
    block_to_files(files, block);

    // seek file1
    int file1idx = seek_file(files, filepath1);
    // handle missing file1
    if (file1idx == -1) {
        std::cout << "FS::cp(err: file \"" << filepath1 << "\" not found)\n";
        return -1;
    }
    dir_entry file1 = files[file1idx];

    // seek file2
    int file2idx = seek_file(files, filepath2);
    // handle missing file2
    if (file2idx == -1) {
        std::cout << "FS::cp(err: file \"" << filepath2 << "\" not found)\n";
        return -1;
    }
    dir_entry file2 = files[file2idx];


    // get content from file2
    std::string content;
    {
        // get first block
        uint16_t blk = file2.first_blk;

        // iterate over file blocks
        char text[BLOCK_SIZE];
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
    }

    // get content from file1
    {
        // get first block
        uint16_t blk = file1.first_blk;

        // iterate over file blocks
        char text[BLOCK_SIZE];
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
    }

    // strip string from null-terminators
    std::string cleaned;
    for (char c : content) {
        if (c != '\0') cleaned += c;
    }
    content = cleaned; // may need memcopy
    
    if (DEBUG) std::cout << "FS::append(" << content << ")\n";
    if (DEBUG) std::cout << "FS::append(" << content.c_str() << ")\n";

    // update file size
    file2.size = file1.size + file2.size;

    // write new content to file2
    {
        // get first block
        uint16_t blk = file2.first_blk;

        // write content and update fat
        blk = file2.first_blk;
        {
            int no_blocks = (int)((file2.size / BLOCK_SIZE) + 1);
            for (int i = 0; i < no_blocks; i++) {
                // convert string slice to block
                char text[BLOCK_SIZE];
                strncpy(text, content.c_str(), BLOCK_SIZE - 1);
                text_to_block(text, block);

                // write block to disk
                disk.write(blk, block);
        
                // if additional iterations
                // sloppy and does not account for shorter files, but that sould not be possible anyways
                if (i + 1 < no_blocks) {
                    // slice string for next iteration
                    content = content.substr(BLOCK_SIZE - 1);
        
                    // find next block for writing
                    // update fat, blockidx and EOF
                    if (fat[blk] == (uint16_t)-1) {
                        int next_blk = free_block(fat);
                        fat[next_blk] = (uint16_t)-1;
                        fat[blk] = next_blk;
                        blk = next_blk;
                    } else {
                        blk = fat[blk];
                    }
                }
            }
        }
    }

    // Save file state
    files[file2idx] = file2;
    files_to_block(files, block);
    disk.write(ROOT_BLOCK, block);

    // Save fat
    fat_to_block(fat, block);
    disk.write(FAT_BLOCK, block);

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
