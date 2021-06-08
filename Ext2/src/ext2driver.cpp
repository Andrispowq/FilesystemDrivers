#include "ext2driver.h"

ext2driver::ext2driver(const std::string& image)
{
	file.open(image, std::ios::in | std::ios::out | std::ios::binary);

	if (!file.is_open())
	{
		std::cerr << "ERROR: " << image << " could not be opened!\n";
	}

	file.seekg(1024);
	superblock = new ext2_superblock();
	file.read((char*)superblock, 1024);

	if (superblock->signature != EXT2_SIGNATURE)
	{
		std::cerr << "ERROR: " << image << " is not an ext2 image!\n";
	}

	blocks_per_block_group = superblock->blocks_per_group;
	inodes_per_block_group = superblock->inodes_per_group;
	block_size = 1024 << superblock->block_size;
	inode_size = superblock->inode_size;
	inodes_per_block = block_size / inode_size;
	
	block_buffer = new uint8_t[block_size];
	inode_buffer = new uint8_t[block_size];

	std::vector<dir_entry> subdirs = ReadDirectory(2);
}

ext2driver::~ext2driver()
{
	delete[] inode_buffer;
	delete[] block_buffer;
	delete superblock;
}

void ext2driver::ReadBlock(uint32_t block, void* data)
{
	file.seekg(block * block_size);
	file.read((char*)data, block_size);
}

void ext2driver::WriteBlock(uint32_t block, void* data)
{
	file.seekg(block * block_size);
	file.write((char*)data, block_size);
}

void ext2driver::ReadInode(uint32_t inode, ext2_inode* data)
{
	uint32_t bg = INODE_BG(inode, inodes_per_block_group);
	ReadBlock(1, block_buffer);
	ext2_bgd* bgd = (ext2_bgd*)block_buffer;
	for (uint32_t i = 0; i < bg; i++)
	{
		bgd++;
	}

	uint32_t index = INODE_INDEX(inode, inodes_per_block_group);
	uint32_t block = INODE_BLOCK(index, inode_size, block_size);
	ReadBlock(bgd->inode_table + block, block_buffer);
	ext2_inode* ino = (ext2_inode*)block_buffer;
	index = index % inodes_per_block;
	for (uint32_t i = 0; i < index; i++)
	{
		ino++;
	}

	memcpy(data, ino, sizeof(ext2_inode));
}

void ext2driver::WriteInode(uint32_t inode, ext2_inode data)
{
	uint32_t bg = INODE_BG(inode, inodes_per_block_group);
	ReadBlock(bg, block_buffer);
	ext2_bgd* bgd = (ext2_bgd*)block_buffer;

	while (bg)
	{
		bgd++;
		bg--;
	}

	uint32_t index = INODE_INDEX(inode, inodes_per_block_group);
	uint32_t block = INODE_BLOCK(index, inode_size, block_size);
	ReadBlock(bgd->inode_bitmap + block, block_buffer);
	ext2_inode* ino = (ext2_inode*)block_buffer;
	index = index % inodes_per_block;

	while (index)
	{
		ino++;
		index--;
	}

	memcpy(ino, &data, sizeof(ext2_inode));
	WriteBlock(bgd->inode_bitmap + block, block_buffer);
}

std::vector<dir_entry> ext2driver::ReadDirectory(uint32_t inode)
{
	std::vector<dir_entry> ret;

	ext2_inode ino;
	ReadInode(inode, &ino);

	if ((ino.type_permissions & 0xF000) != EXT2_TYPE_DIR)
	{
		printf("ERROR: %i is not a directory!\n", inode);
		return ret;
	}

	for (uint32_t i = 0; i < 12; i++)
	{
		uint32_t block = ino.direct[i];
		if (block == 0) break;
		ReadBlock(block, inode_buffer);

		directory_entry* entry = (directory_entry*)inode_buffer;
		while (entry->inode != 0)
		{
			dir_entry ent;
			memset(&ent, 0, sizeof(dir_entry));
			ent.inode = entry->inode;
			ent.size = entry->size;
			ent.type_indicator = entry->type_indicator;
			strcpy((char*)ent.name, (char*)entry->name);

			ret.push_back(ent);
			entry = (directory_entry*)((uint64_t)entry + entry->size);
		}
	}

	return ret;
}
