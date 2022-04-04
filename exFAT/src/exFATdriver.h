#ifndef EX_FAT_DRIVER_H
#define EX_FAT_DRIVER_H

#include "exFATdefs.h"

#include <map>
#include <vector>

#include "Bitmap.h"

namespace exFAT
{
	class exFATDriver
	{
	public:
		exFATDriver(const std::string& image);
		~exFATDriver();

		//if exclude is true, we only give back entries with filter_attributes as their attributes
		//otherwise, we ignore entries that have filter_attributes set
		std::vector<DirEntry> GetDirectories(uint32_t cluster, uint32_t filter_attributes, bool exclude);

		void ModifyDirectoryEntry(uint32_t cluster, const char* name, DirEntry modified);

		int PrepareAddedDirectory(uint32_t cluster);
		void CleanFileEntry(uint32_t cluster, DirEntry entry);

		int DirectorySearch(const char* FilePart, uint32_t cluster, DirEntry* file);
		int DirectoryAdd(uint32_t cluster, DirEntry file);

		int OpenFile(const char* filePath, DirEntry* fileMeta);
		int CreateFile(const char* filePath, DirEntry* fileMeta);
		int DeleteFile(DirEntry entry);

		int ReadFile(DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes);
		int WriteFile(DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes);
		int ResizeFile(DirEntry fileMeta, uint32_t new_size);

	private:
		uint32_t ReadFAT(uint32_t cluster);
		uint32_t WriteFAT(uint32_t cluster, uint32_t value);

		uint32_t ReadCluster(uint32_t cluster, void* buffer);
		uint32_t WriteCluster(uint32_t cluster, void* buffer);

		std::vector<uint32_t> GetClusterChain(uint32_t start);

		uint32_t AllocateClusterChain(uint32_t size);
		void FreeClusterChain(uint32_t start);

		void* ReadClusterChain(uint32_t start, uint32_t& size);
		void WriteClusterChain(uint32_t start, void* buffer, uint32_t size);
		void ResizeClusterChain(uint32_t start, uint32_t new_size);

		void GetDirectoriesOnCluster(uint32_t cluster, std::vector<DirEntry>& entries);
		uint32_t GetClusterFromFilePath(const char* filePath, DirEntry* entry);

		static uint8_t GetMilliseconds();
		static uint16_t GetTime();
		static uint16_t GetDate();

	private:
		std::fstream file;

		uint8_t* temporaryBuffer;
		uint8_t* temporaryBuffer2;
		uint8_t* FATcache;

		char VolumeLabel[11] = { 0 };
		 
		BitmapEntry* bitmapEntry1 = nullptr;
		BitmapEntry* bitmapEntry2 = nullptr;
		Bitmap AllocationBitmap;

		exFAT_BootSector* BootSector;

		uint32_t SectorSize;
		uint32_t SectorsPerCluster;
		uint32_t ClusterSize;

		uint32_t TotalSectors;
		uint32_t TotalClusters;
	};
};

#endif