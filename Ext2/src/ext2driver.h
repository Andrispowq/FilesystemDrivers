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

private:
	void ReadBlock(uint32_t block, void* data);
	void WriteBlock(uint32_t block, void* data);

	void ReadInode(uint32_t inode, ext2_inode* data);
	void WriteInode(uint32_t inode, ext2_inode data);

	std::vector<dir_entry> ReadDirectory(uint32_t inode);

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