#include <iostream>
#include <iomanip>
#include <vector>
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

int seek_in_files(dir_entry *files, std::string name) {
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        if (strcmp(files[i].file_name, name.c_str()) == 0) {
            return i;
        }
    }
    return -1;
}

bool dir_empty(dir_entry *files) {
    // iterate over files
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        // check empty
        if (DEBUG) std::cout << "FS::dir_empty(" << i << " : " << files[i].file_name << ")\n";
        if (files[i].size > (uint32_t)0) {
            // found file in directory
            if (DEBUG) std::cout << "FS::dir_empty(found file: " << i << " : " << files[i].file_name << ")\n";
            return false;
        }
    }
    return true;
}

// Function for splitting path into directories and file
std::vector<std::string> split(std::string path, char delimiter) {
    if (DEBUG) std::cout << "split(" << path << ")\n";

    
    std::vector<std::string> tokens;
    // size_t start = path.at(0) == '/' ? 1 : 0;
    size_t start = 0;
    // find first slash
    size_t end = path.find(delimiter);

    // while string not ended, iterate
    while (end != std::string::npos) {
        // add found token
        std::string subs = path.substr(start, end-start);
        if (subs != "") tokens.push_back(subs);
        // find next slash after updated start;
        start = end + 1;
        end = path.find(delimiter, start);
    }
    tokens.push_back(path.substr(start, end-start)); // trailing token (file name)

    for (size_t i = 0; i < tokens.size(); i++) {
        if (DEBUG) std::cout << "split(): token " << i << ": " << tokens[i] << "\n";
    }

    return tokens;
}

// Function for joining tokens into path
std::string join(std::vector<std::string> tokens, char delimiter) {
    if (DEBUG) std::cout << "join()\n";

    std::string path = "/";
    if (tokens.empty()) return path;
    
    path += tokens[0];
    for (size_t i = 1; i < tokens.size(); i++) {
        path += delimiter + tokens[i];
    }
    return path;
}

// Function for extracting file name from path
std::string path_to_name(std::string path) {
    // Need to handle when path ends with ".."??
    std::vector<std::string> tokens = split(path, '/');
    return tokens.back();
}

// Function to be used when seeking parent directory
std::string pop_path(std::string path) {
    if (DEBUG) std::cout << "pop_path(" << path << ")\n";
    std::vector<std::string> tokens = split(path, '/');
    if (DEBUG) std::cout << "pop_path - pop_back\n";
    tokens.pop_back();
    std::string parentpath = join(tokens, '/');
    if (DEBUG) std::cout << "pop_path - result: " << parentpath << "\n";
    return parentpath;
}

std::string
FS::path_to_abs(std::string path) {
    if (DEBUG) std::cout << "path_to_abs(" << path << ")\n";
    
    std::string abs;
    if (path.at(0) == '/'){
        abs = path;
    } else {
        abs = currentpath == "/" ? currentpath + path : currentpath + "/" + path;
    }    
    if (DEBUG) std::cout << "path_to_abs(abs pre-parse: " << abs << ")\n";
    std::vector<std::string> tokens = split(abs, '/');
    std::vector<std::string> a_tokens;
    if (DEBUG) std::cout << "path_to_abs(DOG: " << join(tokens, '/') << ")\n";

    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i] == "..") {
            if (DEBUG) std::cout << "FS::path_to_abs(pop on " << tokens[i] << ")\n";
            if (a_tokens.size() > 0) a_tokens.pop_back();
            else {
                std::cout << "FS::path_to_abs(parent of \"" << path << "\" not found)\n";
            }
        } else {
            if (DEBUG) std::cout << "FS::path_to_abs(push back on " << tokens[i] << ")\n";
            a_tokens.push_back(tokens[i]);
        }
    }
    abs = join(a_tokens, '/');
    if (DEBUG) std::cout << "path_to_abs(return: " << abs << ")\n";
    return abs;
}

// Function for getting file from path
dir_entry 
FS::path_to_file(std::string ipath) {

    if (DEBUG) std::cout << "FS::path_to_file(" << ipath << ")\n";
    
    std::string path;
    std::string working_path = "/";
    if (ipath == "") {
        path = currentpath;
    }
    // case: absolute path
    else if (ipath.at(0) == '/'){
        path = ipath;
    }
    // case: relative path
    else {
        path = currentpath == "/" ? currentpath + ipath : currentpath + "/" + ipath;
    }
    if (DEBUG) std::cout << "FS::path_to_file(" << path << ")\n";

    if (path == "/") {
        dir_entry root = {
            "root",
            (uint32_t)-1,
            (uint16_t)0,
            (uint8_t)1,
            (uint8_t) READ + WRITE + EXECUTE
        };
        return root;
    }
    
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};

    // split path into directories
    std::vector<std::string> tokens = split(path, '/');

    // get root
    dir_entry root[64];
    disk.read(ROOT_BLOCK, block);
    block_to_files(root, block);

    // iterate over directories
    dir_entry files[64];
    dir_entry file;
    int blockno = 0; // TODO may need to include pass -1 and catch in certain cases
    memcpy(files, root, BLOCK_SIZE); // det files to root
    // for token in tokens get next directory
    if (DEBUG) std::cout << "FS::path_to_file - directory iterative loop\n";
    if (DEBUG) std::cout << "FS::path_to_file - tokens size: " << tokens.size() << "\n";
    for (size_t i = 0; i < tokens.size(); i++) {

        // case: ".."
        if (tokens[i] == "..") {
            if (DEBUG) std::cout << "FS::path_to_file(token case: \"..\")\n";
            // if current is root throw error
            if (working_path == "/") {
                if (DEBUG) std::cout << "FS::path_to_file(err: cannot get parent of root)\n";
                return dir_entry{};
            }

            // recursive fetching of parent
            std::string parent_path = pop_path(working_path);
            dir_entry parent_directory = path_to_file(parent_path);

            // load parent directory files
            disk.read(blockno, block);
            block_to_files(files, block);

            // update working path
            working_path = parent_path;
            if (DEBUG) std::cout << "FS::path_to_file - working_path = " << working_path << "\n";
        }
        // case: normal directory name
        else {
            if (DEBUG) std::cout << "FS::path_to_file(token case: \"" << tokens[i] << "\")\n";
            // check if directory exist in files
            // file name length
            if (tokens[i].length() > 55) {
                if (DEBUG) std::cout << "FS::path_to_file(err: path token \"" << tokens[i] << "\" too long)\n";
                return dir_entry{};
            }
            int fileno = seek_in_files(files, tokens[i]);
            if (DEBUG) std::cout << "path_to_file - fileno = " << fileno << "\n";
            if (fileno == -1) {
                // TODO error message could be better...
                if (DEBUG) std::cout << "FS::path_to_file(directory or file \"" << tokens[i] << "\" not found)\n";
                // maybe loop over tokens and print?
                return dir_entry{};
            }

            // get blockno
            file = files[fileno];
            blockno = (int)file.first_blk;
            if (DEBUG) std::cout << "FS::path_to_file - file_name = " << file.file_name << "\n";
            if (DEBUG) std::cout << "FS::path_to_file - blockno = " << blockno << "\n";
            
            // if more iterations, prepare files for next
            if (i < tokens.size() - 1) {
                // load directory file
                disk.read(blockno, block);
                block_to_files(files, block);
            }
            
            // update working path
            // working_path = working_path + tokens[i] + "/";
            working_path = working_path == "/" ? working_path + tokens[i] : working_path + "/" + tokens[i];
            if (DEBUG) std::cout << "FS::path_to_file - working_path = " << working_path << "\n";
        }
    }

    if (file.type) {
        if (DEBUG) std::cout << "FS::path_to_file - returning directory: " << file.file_name << "\n";   
    } else {
        if (DEBUG) std::cout << "FS::path_to_file - returning file: " << file.file_name << "\n";   
    }

    return file;
}

// Function for computing file index in directory from path
int 
FS::path_to_file_idx(std::string filepath) {

    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};

    std::string parent_path = pop_path(path_to_abs(filepath));
    dir_entry parent_dir = path_to_file(parent_path);
    if (!(parent_dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::path_to_file_idx(unable to find parent directory)\n";
        return -1;
    }
    int blockno = parent_dir.first_blk;
    dir_entry files[64];
    disk.read(blockno, block);
    block_to_files(files, block);

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        std::string name = path_to_name(filepath);
        if (strcmp(files[i].file_name, name.c_str()) == 0) {
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

    // reset current path
    currentpath = "/";
    
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    if (DEBUG) std::cout << "FS::create(" << filepath << ")\n";
    
    if (filepath == "..") {
        std::cout << "FS::create(filepath \"..\" is not valid)\n";
        return -1;
    }

    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(FAT_BLOCK, block);
    block_to_fat(fat, block);

    // GET FILES from path parent directory
    std::string parent_path = pop_path(path_to_abs(filepath));
    dir_entry parent_dir = path_to_file(parent_path);
    if (!(parent_dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::create(unable to find parent directory)\n";
        return -1;
    }
    int blockno = parent_dir.first_blk;
    dir_entry files[64];
    disk.read(blockno, block);
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
    std::string name = path_to_name(filepath);
    strncpy(file.file_name, name.c_str(), 55); // allow names of length 55

    // Get next free block
    int blk = free_block(fat);
    file.first_blk = blk; // write to file header
    fat[blk] = (uint16_t)-1; // set EOF

    // Get content of file
    std::string content;
    std::string s;
    do {
        std::getline(std::cin, s);
        // pad with new line or null terminator
        if (!s.empty()) content = content + s + '\n';
        else content[content.size() - 1] = '\0';
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
    disk.write(blockno, block);

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

    // GET FILES from path parent directory
    std::string parent_path = pop_path(path_to_abs(filepath));;
    dir_entry parent_dir = path_to_file(parent_path);
    if (!(parent_dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::cat(unable to find parent directory)\n";
        return -1;
    }
    int blockno = parent_dir.first_blk;
    dir_entry files[64];
    disk.read(blockno, block);
    block_to_files(files, block);

    // file length
    if (filepath.length() > 55) {
        std::cout << "FS::cat(err: file \"" << filepath << "\" too long)\n";
        return -1;
    }

    // seek file
    dir_entry file = path_to_file(path_to_abs(filepath));
    // handle missing file
    if (!(file.size > (uint32_t)0)) { // check existance
        std::cout << "FS::cat(err: file \"" << filepath << "\" not found)\n";
        return -1;
    }
    
    // check file type
    if (file.type) {
        std::cout << "FS::cat(err: file \"" << filepath << "\" of type directory)\n";
        return -1;
    }

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
    
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};

    // GET FILES from path parent directory
    dir_entry parent_dir = path_to_file(currentpath);
    if (!(parent_dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::ls(unable to find parent directory)\n";
        return -1;
    }
    int blockno = parent_dir.first_blk;
    dir_entry files[64];
    disk.read(blockno, block);
    block_to_files(files, block);

    std::cout << std::left
        << std::setw(58) << "name"
        << std::setw(7) << "type"
        << std::setw(12) << "size"
        << "\n";
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
        // found file
        if ((files[i].size > (uint32_t)0)) {
            dir_entry file = files[i];
            std::string size = file.type ? "-" : std::to_string(file.size);
            std::string type = file.type ? "dir" : "file";
            std::cout << std::left
                << std::setw(58) << file.file_name
                << std::setw(7) << type
                << std::setw(12) << size
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

    // GET FILES from path parent directory
    std::string src_parent_path = pop_path(path_to_abs(sourcepath));
    dir_entry source_dir = path_to_file(src_parent_path);
    if (!(source_dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::cp(unable to find parent directory)\n";
        return -1;
    }
    int blockno = source_dir.first_blk;
    dir_entry files[64];
    disk.read(blockno, block);
    block_to_files(files, block);

    // file name length
    if (sourcepath.length() > 55) {
        std::cout << "FS::cp(err: file \"" << sourcepath << "\" too long)\n";
        return -1;
    }
    if (destpath.length() > 55) {
        std::cout << "FS::cp(err: file \"" << destpath << "\" too long)\n";
        return -1;
    }

    // seek file
    dir_entry sourcefile = path_to_file(path_to_abs(sourcepath));
    // handle missing file
    if (!(sourcefile.size > (uint32_t)0)) { // check existance
        std::cout << "FS::cp(err: file \"" << sourcepath << "\" not found)\n";
        return -1;
    }

    // check source type
    if (sourcefile.type) {
        std::cout << "FS::cp(err: file \"" << sourcepath << "\" of type directory)\n";
        return -1;
    }

    // seek destination
    dir_entry directory = path_to_file(path_to_abs(destpath));
    // handle missing file
    bool isdir = false;
    if ((directory.size > (uint32_t)0)) { // check existance
        if (directory.type == 0) { // if file
            std::cout << "FS::cp(err: file \"" << destpath << "\" already exists)\n";
            return -1;
        } else { // if directory
            // swap files
            blockno = directory.first_blk;
            disk.read(blockno, block);
            block_to_files(files, block);

            // save directory state
            isdir = true;
        }
    }

    // Find space for file in directory
    int destidx = free_file(files);

    if (destidx == -1) {
        std::cout << "FS::cp(err: directory full)\n";
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
        std::string new_name = isdir ? sourcefile.file_name : path_to_name(destpath);
        strncpy(destfile.file_name, new_name.c_str(), 55); // allow names of length 55

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
        disk.write(blockno, block);

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

    // GET FILES from path parent directory
    std::string src_parent_path = pop_path(path_to_abs(sourcepath));
    dir_entry source_dir = path_to_file(src_parent_path);
    if (!(source_dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::mv(unable to find parent directory)\n";
        return -1;
    }
    int blockno = source_dir.first_blk;
    dir_entry files[64];
    disk.read(blockno, block);
    block_to_files(files, block);

    // file name length
    if (sourcepath.length() > 55) {
        std::cout << "FS::mv(err: file \"" << sourcepath << "\" too long)\n";
        return -1;
    }
    if (destpath.length() > 55) {
        std::cout << "FS::mv(err: destination \"" << destpath << "\" too long)\n";
        return -1;
    }

    // seek file
    dir_entry sourcefile = path_to_file(path_to_abs(sourcepath));
    int sourceidx = path_to_file_idx(sourcepath);
    // handle missing file
    if (!(sourcefile.size > (uint32_t)0)) { // check existance
        std::cout << "FS::mv(err: file \"" << sourcepath << "\" not found)\n";
        return -1;
    }

    // check source type
    if (sourcefile.type) {
        std::cout << "FS::mv(err: file \"" << sourcepath << "\" of type directory)\n";
        return -1;
    }

    // save information in case of succesful move, for removal from source directory
    dir_entry sourcefiles[64];
    memcpy(sourcefiles, files, BLOCK_SIZE);
    int sourceblockno = blockno;

    // seek destination
    dir_entry directory = path_to_file(path_to_abs(destpath));
    // handle missing file
    bool isdir = false;
    if ((directory.size > (uint32_t)0)) { // check existance
        if (directory.type == 0) { // if file
            // handle duplicate name
            std::cout << "FS::mv(err: file \"" << destpath << "\" already exists)\n";
            return -1;
        } else { // if directory, move file
            // swap files
            blockno = directory.first_blk;
            disk.read(blockno, block);
            block_to_files(files, block);

            // Find space for file in directory
            int destidx = free_file(files);

            if (is_duplicate_name(files, sourcefile.file_name)) {
                std::cout << "FS::mv(err: file \"" << sourcefile.file_name << "\" already exists)\n";
                return -1;
            }
            // handle full directory
            if (destidx == -1) {
                std::cout << "FS::mv(err: directory full)\n";
                return -1;
            }

            // write file to new directory
            files[destidx] = sourcefile;

            // remove file from old directory
            sourcefiles[sourceidx] = dir_entry{};

            // save directory state
            isdir = true;
        }
    } else { // rename
        if (DEBUG) std::cout << "FS::mv(renaming file)\n";
        // strncpy with sizeof to limit string size
        std::string new_name = isdir ? sourcefile.file_name : path_to_name(destpath);
        if (DEBUG) std::cout << "FS::mv(new name: " << new_name << ")\n";
        if (DEBUG) std::cout << "FS::mv(srcidx: " << sourceidx << ")\n";
        strncpy(sourcefile.file_name, new_name.c_str(), 55); // allow names of length 55
        
        // write new file name to files
        files[sourceidx] = sourcefile;
    }

    // Save file in new location
    files_to_block(files, block);
    disk.write(blockno, block);

    // Remove old file if move directory
    if (isdir) {
        files_to_block(sourcefiles, block);
        disk.write(sourceblockno, block);
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

    // GET FILES from path parent directory
    std::string parent_path = pop_path(path_to_abs(filepath));
    dir_entry parent_dir = path_to_file(parent_path);
    if (!(parent_dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::rm(unable to find parent directory)\n";
        return -1;
    }
    int blockno = parent_dir.first_blk;
    dir_entry files[64];
    disk.read(blockno, block);
    block_to_files(files, block);

    // file name length
    if (filepath.length() > 55) {
        std::cout << "FS::rm(err: file \"" << filepath << "\" too long)\n";
        return -1;
    }

    // seek file
    dir_entry file = path_to_file(path_to_abs(filepath));
    int fileidx = path_to_file_idx(filepath);
    // handle missing file
    if (!(file.size > (uint32_t)0)) { // check existance
        std::cout << "FS::rm(err: file \"" << filepath << "\" not found)\n";
        return -1;
    }
    
    // save first block
    uint16_t blk = file.first_blk;

    // case: directory
    if (file.type) {
        // load directory
        dir_entry directory[64];
        disk.read(blk, block);
        block_to_files(directory, block);
    
        bool empty = dir_empty(directory);
        if (!empty) {
            std::cout << "FS::rm(err: directory \"" << filepath << "\" not empty)\n";
            return -1;
        } else {
            // remove directory entry
            files[fileidx] = dir_entry{};
        }

    }
    // case: file
    else {
        // remove directory entry
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
    disk.write(blockno, block);

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

    // GET FILES from path parent directory
    std::string parent_path = pop_path(path_to_abs(filepath2));
    dir_entry parent_dir = path_to_file(parent_path);
    if (!(parent_dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::append(unable to find parent directory)\n";
        return -1;
    }
    int blockno = parent_dir.first_blk;
    dir_entry files[64];
    disk.read(blockno, block);
    block_to_files(files, block);

    // file name length
    if (filepath1.length() > 55) {
        std::cout << "FS::append(err: file \"" << filepath1 << "\" too long)\n";
        return -1;
    }
    if (filepath2.length() > 55) {
        std::cout << "FS::append(err: file \"" << filepath2 << "\" too long)\n";
        return -1;
    }

    // seek file1
    dir_entry file1 = path_to_file(path_to_abs(filepath1));
    // handle missing file
    if (!(file1.size > (uint32_t)0)) { // check existance
        std::cout << "FS::append(err: file \"" << filepath1 << "\" not found)\n";
        return -1;
    }

    // check file1 type
    if (file1.type) {
        std::cout << "FS::append(err: file \"" << filepath1 << "\" of type directory)\n";
        return -1;
    }

    // seek file2
    dir_entry file2 = path_to_file(path_to_abs(filepath2));
    int file2idx = path_to_file_idx(filepath2);
    // handle missing file
    if (!(file2.size > (uint32_t)0)) { // check existance
        std::cout << "FS::cp(err: file \"" << filepath2 << "\" not found)\n";
        return -1;
    }

    // check file2 type
    if (file2.type) {
        std::cout << "FS::append(err: file \"" << filepath2 << "\" of type directory)\n";
        return -1;
    }

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
    // pad file with new line, null terminator is removed later
    content = content + '\n';

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
    content = content + '\0'; // add new null terminator following combining
    
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
    disk.write(blockno, block);

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
    if (DEBUG) std::cout << "FS::mkdir(" << dirpath << ")\n";

    // LOAD FAT
    uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    disk.read(FAT_BLOCK, block);
    block_to_fat(fat, block);

    // GET FILES from path parent directory
    std::string parent_path = pop_path(path_to_abs(dirpath));
    dir_entry parent_dir = path_to_file(parent_path);
    if (!(parent_dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::mkdir(unable to find parent directory)\n";
        return -1;
    }
    int blockno = parent_dir.first_blk;
    dir_entry files[64];
    disk.read(blockno, block);
    block_to_files(files, block);

    // Find space for file in directory
    int fileidx = free_file(files);

    if (fileidx == -1) {
        std::cout << "FS::mkdir(err: directory full)\n";
        return -1;
    }

    if (dirpath.length() > 55) {
        std::cout << "FS::mkdir(err: directory name \"" << dirpath << "\" too long)\n";
        return -1;
    }

    // seek file
    dir_entry dir = path_to_file(dirpath);
    if ((dir.size > (uint32_t)0)) { // check existance
        std::cout << "FS::mkdir(err: file or directory \"" << dirpath << "\" already exists)\n";
        return -1;
    }

    // Create file
    dir_entry file;

    // strncpy with sizeof to limit string size
    std::string new_name = path_to_name(dirpath);
    strncpy(file.file_name, new_name.c_str(), 55); // allow names of length 55

    // Get next free block
    int blk = free_block(fat);
    file.first_blk = blk; // write to file header
    fat[blk] = (uint16_t)-1; // set EOF

    // Save size
    file.size = (uint32_t)-1;

    file.type = (uint8_t)1; // Set type to file. [0=file, 1=directory]
    file.access_rights = (uint8_t)(READ + WRITE + EXECUTE); // read (0x04) + write (0x02) + execute (0x01)

    // Clear block
    block = new uint8_t [BLOCK_SIZE] {0};
    disk.write(blk, block);

    // Save file
    files[fileidx] = file;
    files_to_block(files, block);
    disk.write(blockno, block);

    // Save fat
    fat_to_block(fat, block);
    disk.write(FAT_BLOCK, block);

    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    if (DEBUG) std::cout << "FS::cd(" << dirpath << ")\n";

    // // LOAD FAT
    // uint16_t *fat = new uint16_t [BLOCK_SIZE / 2] {0};
    // uint8_t *block = new uint8_t [BLOCK_SIZE] {0};
    // disk.read(FAT_BLOCK, block);
    // block_to_fat(fat, block);

    // // GET FILES from path parent directory
    // std::string parent_path = pop_path(path_to_abs(dirpath));
    // dir_entry parent_dir = path_to_file(parent_path);
    // if (!(parent_dir.size > (uint32_t)0)) { // check existance
    //     std::cout << "FS::cd(unable to find parent directory)\n";
    //     return -1;
    // }
    // int blockno = parent_dir.first_blk;
    // dir_entry files[64];
    // disk.read(blockno, block);
    // block_to_files(files, block);

    // file name length
    if (dirpath.length() > 55) {
        std::cout << "FS::cd(directory \"" << dirpath << "\" too long)\n";
        return -1;
    }

    // seek file
    dir_entry directory = path_to_file(dirpath);
    // handle missing file
    if (!(directory.size > (uint32_t)0)) { // check existance
        std::cout << "FS::cd(file \"" << dirpath << "\" not found)\n";
        return -1;
    }

    // check if type directory
    if (directory.type != (uint8_t)1) { // if file
        std::cout << "FS::cd(file \"" << dirpath << "\" not of directory type)\n";
        return -1;
    }

    currentpath = path_to_abs(dirpath);

    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    if (DEBUG) std::cout << "FS::pwd()\n";

    std::cout << currentpath << "\n";
    
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
