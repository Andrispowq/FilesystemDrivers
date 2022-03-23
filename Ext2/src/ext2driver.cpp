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

	/*std::vector<ext2_DirEntry> root = GetDirectories(2);

	for (auto& elem : root)
	{
		if ((elem.type_indicator & EXT2_TYPE_DIR) == EXT2_TYPE_DIR)
		{

			std::vector<ext2_DirEntry> root_ = GetDirectories(elem.inode);

			std::cout << "Under " << elem.name << '\n';
			for (auto& elem_ : root_)
			{
				std::cout << elem_.name << ' ';
			}

			std::cout << '\n';
		}
		else
		{
			std::cout << elem.name << '\n';
		}
	}*/

}

ext2driver::~ext2driver()
{
	delete[] inode_buffer;
	delete[] block_buffer;
	delete superblock;
}

std::vector<ext2_DirEntry> ext2driver::GetDirectories(uint32_t inode)
{
	std::vector<ext2_DirEntry> entries;

	ext2_inode ino;
	ReadInode(inode, &ino);

	if ((ino.type_permissions & 0xF000) != EXT2_TYPE_DIR)
	{
		printf("ERROR: %i is not a directory!\n", inode);
		return entries;
	}

	for (uint32_t i = 0; i < 12; i++)
	{
		uint32_t block = ino.direct[i];
		if (block == 0) break;
		ReadBlock(block, inode_buffer);

		directory_entry* entry = (directory_entry*)inode_buffer;
		uint32_t totalSize = 0;
		while (totalSize < block_size)
		{
			if (entry->inode == 0)
			{
				break;
			}

			entries.push_back(ToDirEntry(entry));
			totalSize += entry->size;
			entry = (directory_entry*)((uint64_t)entry + entry->size);
		}
	}

	return entries;
}

int ext2driver::PrepareAddedDirectory(uint32_t inode)
{
	ext2_inode ino;
	ReadInode(inode, &ino);

	if ((ino.type_permissions & 0xF000) != EXT2_TYPE_DIR)
	{
		printf("ERROR: %i is not a directory!\n", inode);
		return -1;
	}

	for (uint32_t i = 0; i < 12; i++)
	{
		uint32_t block = ino.direct[i];
		if (block == 0) break;
		ReadBlock(block, inode_buffer);

		directory_entry* entry = (directory_entry*)inode_buffer;

		entry->inode = inode; //parent
		entry->name_length_low = 1;
		strcpy((char*)entry->name, ".");
		entry->size = 8 + 2; //2 bytes of name + struct size
		entry = (directory_entry*)((uint64_t)entry + entry->size);

		entry->inode = 0; //parent
		entry->name_length_low = 2;
		strcpy((char*)entry->name, "..");
		entry->size = 8 + 3; //2 bytes of name + struct size
		entry = (directory_entry*)((uint64_t)entry + entry->size);
	}

	WriteInode(inode, ino);
}

void ext2driver::CleanFileEntry(uint32_t inode, ext2_DirEntry entry)
{

}

/*int ext2driver::DirectorySearch(const char* FilePart, uint32_t cluster, ext2_direntry* file)
{
	return 0;
}

int ext2driver::DirectoryAdd(uint32_t cluster, ext2_direntry file)
{
	return 0;
}

int ext2driver::OpenFile(const char* filePath, ext2_direntry* fileMeta)
{
	return 0;
}

int ext2driver::CreateFile(const char* filePath, ext2_direntry* fileMeta)
{
	return 0;
}

int ext2driver::DeleteFile(ext2_direntry fileMeta)
{
	return 0;
}

int ext2driver::ReadFile(ext2_direntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes)
{
	char fileNamePart[256];

	uint32_t iterator = 1;
	uint32_t start = 1;
	uint32_t inode = 2;

	std::vector<dir_entry> dir;
	GetDirectories(inode, dir);

	for (iterator = 2; path[iterator - 1] != 0; iterator++)
	{
		if (path[iterator] == '/' || path[iterator] == 0)
		{
			memset(fileNamePart, 0, 256);
			memcpy(fileNamePart, (void*)((uint64_t)path + start), iterator - start);

			bool found = false;
			for (auto elem : dir)
			{
				if (strcmp((const char*)elem.name, fileNamePart) == 0)
				{
					start = iterator + 1;
					inode = elem.inode;

					if (path[iterator] != 0)
					{
						GetDirectories(inode, dir);
					}

					found = true;
					break;
				}
			}

			if (!found)
				return;
		}
	}

	ext2_inode data;
	ReadInode(inode, &data);

	ReadBlock(data.direct[0], buffer);
	return;
	return 0;
}

int ext2driver::WriteFile(ext2_direntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes)
{
	return 0;
}

int ext2driver::ResizeFile(ext2_direntry fileMeta, uint32_t new_size)
{
	return 0;
}*/

ext2_DirEntry ext2driver::ToDirEntry(directory_entry* inode)
{
	ext2_DirEntry entry;
	memset(&entry, 0, sizeof(ext2_DirEntry));

	strcpy(entry.name, (const char*)inode->name);
	entry.inode = inode->inode;
	entry.size = inode->size;
	entry.type_indicator = inode->type_indicator;

	ReadInode(inode->inode, &entry.inode_data);

	return entry;
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
