#ifndef EXT2_H
#define EXT2_H

#include "ext2defs.h"

#define INODE_BG(in, in_per_g) ((in - 1) / in_per_g)
#define INODE_INDEX(in, in_per_g) ((in - 1) % in_per_g)
#define INODE_BLOCK(in_in_bg, in_size, block_size) ((in_in_bg * in_size) / block_size)

#include <vector>

namespace ext2
{
	class ext2driver
	{
	public:
		ext2driver(const std::string& image);
		~ext2driver();

		std::vector<DirEntry> GetDirectories(uint32_t inode);

		void ModifyDirectoryEntry(uint32_t inode, const char* name, DirEntry modified);

		int PrepareAddedDirectory(uint32_t inode);
		void CleanFileEntry(uint32_t inode, DirEntry entry);

		int DirectorySearch(const char* FilePart, uint32_t inode, DirEntry* file);
		int DirectoryAdd(uint32_t inode, DirEntry file);

		int OpenFile(const char* filePath, DirEntry* fileMeta);
		int CreateFile(const char* filePath, DirEntry* fileMeta);
		int DeleteFile(DirEntry fileMeta);

		int ReadFile(DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes);
		int WriteFile(DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes);
		int ResizeFile(DirEntry fileMeta, uint32_t new_size);

	private:
		DirEntry ToDirEntry(directory_entry* inode);

		void ReadBlock(uint32_t block, void* data);
		void WriteBlock(uint32_t block, void* data);

		void ReadInode(uint32_t inode, ext2_inode* data);
		void WriteInode(uint32_t inode, ext2_inode data);

		void GetDirectoriesOnInode(uint32_t inode, std::vector<DirEntry>& entries);
		uint32_t GetInodeFromFilePath(const char* filePath, DirEntry* entry);

		uint32_t GetBlockOnInode(ext2_inode inode, uint64_t block_index);

		uint64_t GetSize(ext2_inode inode);

	private:
		std::fstream file;

		SuperBlock* superblock;
		uint8_t* block_buffer;
		uint8_t* inode_buffer;
		uint8_t* indirect_buffer;

		uint32_t blocks_per_block_group;
		uint32_t inodes_per_block_group;
		uint32_t block_size;
		uint32_t inode_size;
		uint32_t inodes_per_block;

		bool read_only = false;
	};
};

#endif