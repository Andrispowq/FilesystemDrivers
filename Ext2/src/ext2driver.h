#ifndef EXT2_H
#define EXT2_H

#include "ext2defs.h"

#define INODE_BG(in, in_per_g) ((in - 1) / in_per_g)
#define INODE_INDEX(in, in_per_g) ((in - 1) % in_per_g)
#define INODE_BLOCK(in_in_bg, in_size, block_size) ((in_in_bg * in_size) / block_size)

#include <vector>

class ext2driver
{
public:
	ext2driver(const std::string& image);
	~ext2driver();

	std::vector<ext2_DirEntry> GetDirectories(uint32_t inode);

	int PrepareAddedDirectory(uint32_t inode);
	void CleanFileEntry(uint32_t inode, ext2_DirEntry entry);

	int DirectorySearch(const char* FilePart, uint32_t cluster, ext2_DirEntry* file);
	int DirectoryAdd(uint32_t cluster, ext2_DirEntry file);

	int OpenFile(const char* filePath, ext2_DirEntry* fileMeta);
	int CreateFile(const char* filePath, ext2_DirEntry* fileMeta);
	int DeleteFile(ext2_DirEntry fileMeta);

	int ReadFile(ext2_DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes);
	int WriteFile(ext2_DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes);
	int ResizeFile(ext2_DirEntry fileMeta, uint32_t new_size);

private:
	ext2_DirEntry ToDirEntry(directory_entry* inode);

	void ReadBlock(uint32_t block, void* data);
	void WriteBlock(uint32_t block, void* data);

	void ReadInode(uint32_t inode, ext2_inode* data);
	void WriteInode(uint32_t inode, ext2_inode data);

private:
	std::fstream file;

	ext2_superblock* superblock;
	uint8_t* block_buffer;
	uint8_t* inode_buffer;

	uint32_t blocks_per_block_group;
	uint32_t inodes_per_block_group;
	uint32_t block_size;
	uint32_t inode_size;
	uint32_t inodes_per_block;
};

#endif