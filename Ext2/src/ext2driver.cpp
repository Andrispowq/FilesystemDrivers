#include "ext2driver.h"

namespace ext2
{
	ext2driver::ext2driver(const std::string& image)
	{
		file.open(image, std::ios::in | std::ios::out | std::ios::binary);

		if (!file.is_open())
		{
			std::cerr << "ERROR: " << image << " could not be opened!\n";
		}

		file.seekg(1024);
		superblock = new SuperBlock();
		file.read((char*)superblock, 1024);

		if (superblock->signature != EXT2_SIGNATURE)
		{
			std::cerr << "ERROR: " << image << " is not an ext2 image!\n";
		}
		//perform the necessary checks
		if (superblock->signature != EXT2_SIGNATURE)
		{
			std::cerr << "ERROR: not ext2 filesystem!!!\n";
			goto error;
		}

		if (superblock->fs_state != (uint16_t)ext2::FilesystemState::clean)
		{
			switch (superblock->error_protocol)
			{
			case (uint16_t)ext2::ErrorProtocol::ignore:
				break;
			case (uint16_t)ext2::ErrorProtocol::remount_as_read_only:
				read_only = true;
				break;
			case (uint16_t)ext2::ErrorProtocol::kernel_panic:
				std::cerr << "ERROR: the filesystem is in not in Clean state!!!\n";
				goto error;
				break;
			}
		}

		blocks_per_block_group = superblock->blocks_per_group;
		inodes_per_block_group = superblock->inodes_per_group;
		block_size = 1024 << superblock->block_size;
		inode_size = (superblock->version_major == 0) ? 128 : superblock->inode_size;
		inodes_per_block = block_size / inode_size;

		block_buffer = new uint8_t[block_size];
		inode_buffer = new uint8_t[block_size];
		indirect_buffer = new uint8_t[block_size];

	error:
		return;
	}

	ext2driver::~ext2driver()
	{
		delete[] inode_buffer;
		delete[] block_buffer;
		delete[] indirect_buffer;

		delete superblock;
	}

	std::vector<DirEntry> ext2driver::GetDirectories(uint32_t inode)
	{
		std::vector<DirEntry> entries;
		GetDirectoriesOnInode(inode, entries);

		return entries;
	}

	void ext2driver::ModifyDirectoryEntry(uint32_t inode, const char* name, DirEntry modified)
	{
		ext2_inode ino;
		ReadInode(inode, &ino);

		uint32_t index = 0;
		while (true)
		{
			uint32_t block = GetBlockOnInode(ino, index++);
			if (block == 0) break;
			ReadBlock(block, inode_buffer);

			directory_entry* entry = (directory_entry*)inode_buffer;

			if (modified.type_indicator == 0)
			{
				//We want to delete the entry altogether
				memset(entry, 0, entry->size);
			}
			else
			{
				entry->inode = modified.inode;
				entry->size = modified.size;
				entry->type_indicator = modified.type_indicator;
			}
		}

		WriteInode(inode, ino);
		WriteInode(modified.inode, modified.inode_data);
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

		uint32_t index = 0;
		while(true)
		{
			uint32_t block = GetBlockOnInode(ino, index++);
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

	void ext2driver::CleanFileEntry(uint32_t inode, DirEntry entry)
	{
		entry.type_indicator = 0;
		ModifyDirectoryEntry(inode, entry.name, entry);
	}

	int ext2driver::DirectorySearch(const char* FilePart, uint32_t inode, DirEntry* file)
	{
		if (inode < 2)
		{
			return -1;
		}

		std::vector<DirEntry> entries;
		GetDirectoriesOnInode(inode, entries);

		for (auto entry : entries)
		{
			if (strcmp(entry.name, FilePart) == 0)
			{
				if (file != nullptr)
				{
					*file = entry;
				}

				return 0;
			}
		}

		return -2;
	}

	/*int ext2driver::DirectoryAdd(uint32_t inode, DirEntry file)
	{
		return 0;
	}*/

	DirEntry ext2driver::ToDirEntry(directory_entry* inode)
	{
		DirEntry entry;
		memset(&entry, 0, sizeof(DirEntry));

		ext2_inode ino;
		ReadInode(inode->inode, &ino);

		strcpy(entry.name, (const char*)inode->name);
		entry.inode = inode->inode;
		entry.size = GetSize(ino);
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

	void ext2driver::GetDirectoriesOnInode(uint32_t inode, std::vector<DirEntry>& entries)
	{
		ext2_inode ino;
		ReadInode(inode, &ino);

		if ((ino.type_permissions & 0xF000) != EXT2_TYPE_DIR)
		{
			printf("ERROR: %i is not a directory!\n", inode);
			return;
		}

		uint32_t offset = 0;
		uint32_t index = 0;
		while (true)
		{
			uint32_t block = GetBlockOnInode(ino, index++);
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

				DirEntry _entry = ToDirEntry(entry);
				_entry.parentInode = inode;
				_entry.offsetInParentInode = offset;
				entries.push_back(_entry);

				totalSize += entry->size;
				entry = (directory_entry*)((uint64_t)entry + entry->size);
				offset++;
			}
		}
	}

	uint32_t ext2driver::GetInodeFromFilePath(const char* filePath, DirEntry* entry)
	{
		char fileNamePart[256];
		uint16_t start = 2; //Skip the "~/" part
		uint32_t active_inode = EXT2_ROOT_INODE; //Start from root
		DirEntry fileInfo;

		uint32_t iterator = 2;
		if (strcmp(filePath, "~") == 0)
		{
			fileInfo.name[0] = '~';
			fileInfo.inode = active_inode;
			fileInfo.size = 0;
			fileInfo.type_indicator = EXT2_TYPE_DIR;
			ReadInode(2, &fileInfo.inode_data);
		}
		else
		{
			for (iterator = 2; filePath[iterator - 1] != 0; iterator++)
			{
				if (filePath[iterator] == '/' || filePath[iterator] == 0)
				{
					memset(fileNamePart, 0, 256);
					memcpy(fileNamePart, (void*)((uint64_t)filePath + start), iterator - start);

					if (fileNamePart[0] == 0) //if you try to open a directory you may add a '/' at the end, this avoids problems in that scenario
					{
						continue;
					}

					int retVal = DirectorySearch(fileNamePart, active_inode, &fileInfo);

					if (retVal != 0)
					{
						return retVal;
					}

					start = iterator + 1;
					active_inode = fileInfo.inode;
				}
			}
		}

		if (entry)
		{
			*entry = fileInfo;
		}

		return active_inode;
	}

	uint32_t ext2driver::GetBlockOnInode(ext2_inode inode, uint64_t block_index)
	{
		uint32_t pointers_per_block = block_size / 4;

		if (block_index < 12)
		{
			return inode.direct[block_index];
		}
		else if(block_index < (12 + pointers_per_block))
		{
			uint32_t indirect_index = block_index - 12;
			ReadBlock(inode.single_indirect, indirect_buffer);
			return ((uint32_t*)indirect_buffer)[indirect_index];
		}
		else if (block_index < (12 + pointers_per_block + pointers_per_block * pointers_per_block))
		{
			uint32_t indirect_index = block_index - 12 - pointers_per_block;
			ReadBlock(inode.double_indirect, indirect_buffer);
			uint32_t single_indirect = ((uint32_t*)indirect_buffer)[indirect_index];

			indirect_index = block_index - 12;
			ReadBlock(single_indirect, indirect_buffer);
			return ((uint32_t*)indirect_buffer)[indirect_index];
		}
		else if (block_index < (12 + pointers_per_block + pointers_per_block * pointers_per_block + pointers_per_block * pointers_per_block * pointers_per_block))
		{
			uint32_t indirect_index = block_index - 12 - pointers_per_block - pointers_per_block * pointers_per_block;
			ReadBlock(inode.triple_indirect, indirect_buffer);
			uint32_t double_indirect = ((uint32_t*)indirect_buffer)[indirect_index];

			indirect_index = block_index - 12 - pointers_per_block;
			ReadBlock(double_indirect, indirect_buffer);
			uint32_t single_indirect = ((uint32_t*)indirect_buffer)[indirect_index];

			indirect_index = block_index - 12;
			ReadBlock(single_indirect, indirect_buffer);
			return ((uint32_t*)indirect_buffer)[indirect_index];
		}

		return 0;
	}

	uint64_t ext2driver::GetSize(ext2_inode inode)
	{
		if (superblock->features_needed_else_read_only & EXT2_FEATURE_64_BIT_SIZE)
		{
			return (uint64_t)inode.size_low | (((uint64_t)inode.size_high) << 32);
		}
		else
		{
			return inode.size_low;
		}
	}

	int ext2driver::OpenFile(const char* filePath, DirEntry* fileMeta)
	{
		if (fileMeta == nullptr)
		{
			return -1;
		}

		int ret = GetInodeFromFilePath(filePath, fileMeta);
		if (ret < 0)
		{
			return ret;
		}

		return 0;
	}

	int ext2driver::ReadFile(DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes)
	{
		if ((fileMeta.type_indicator & 0xF000) == EXT2_TYPE_DIR)
		{
			return -3;
		}

		if ((bytes + offset) > fileMeta.size)
		{
			bytes = fileMeta.size - offset;
		}

		uint64_t bytes_so_far = 0;
		uint8_t* buff = (uint8_t*)buffer;
		uint32_t index = 0;
		while (true)
		{
			uint32_t block = GetBlockOnInode(fileMeta.inode_data, index++);
			if (block == 0) break;

			if (bytes_so_far < offset)
			{
				bytes_so_far += block_size;
				continue;
			}

			uint64_t toRead = (bytes + offset) - bytes_so_far;
			if (toRead > block_size)
			{
				toRead = block_size;
				ReadBlock(block, buff);
			}
			else
			{
				uint8_t* temporary = new uint8_t[block_size];
				ReadBlock(block, temporary);
				memcpy(buff, temporary, toRead);
				delete[] temporary;
			}

			buff += block_size;
			bytes_so_far += toRead;
		}

		return 0;
	}
};
