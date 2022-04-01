#include "exFATdriver.h"

namespace exFAT
{
	exFATDriver::exFATDriver(const std::string& image)
	{
		file.open(image, std::ios::in | std::ios::out | std::ios::binary);

		if (!file.is_open())
		{
			std::cerr << "ERROR: " << image << " could not be opened!\n";
		}

		BootSector = new exFAT_BootSector();
		file.read((char*)BootSector, sizeof(exFAT_BootSector));

		SectorSize = (1 << BootSector->SectorShift);
		SectorsPerCluster = (1 << BootSector->ClusterShift);
		ClusterSize = (1 << (BootSector->SectorShift + BootSector->ClusterShift));

		TotalSectors = BootSector->VolumeLength;
		TotalClusters = TotalSectors / SectorsPerCluster;

		temporaryBuffer = new uint8_t[ClusterSize];
		temporaryBuffer2 = new uint8_t[ClusterSize];

		FATcache = new uint8_t[BootSector->FATLength * SectorSize];

		if (((BootSector->Flags & EX_FAT_USE_SECOND_FAT) == EX_FAT_USE_SECOND_FAT) && (BootSector->NumberOfFATs == 2))
		{
			file.seekg((BootSector->FATOffset + BootSector->FATLength) * SectorSize);
			file.read((char*)FATcache, BootSector->FATLength * SectorSize);
		}
		else
		{
			file.seekg(BootSector->FATOffset * SectorSize);
			file.read((char*)FATcache, BootSector->FATLength * SectorSize);
		}
	}

	exFATDriver::~exFATDriver()
	{
		file.seekg(0);
		file.write((const char*)BootSector, 512);

		file.seekg(BootSector->FATOffset * SectorSize);
		file.write((const char*)FATcache, BootSector->FATLength * SectorSize);

		//copy the first FAT onto the other ones upon closing the FS
		for (uint32_t i = 1; i < BootSector->NumberOfFATs; i++)
		{
			file.seekg((BootSector->FATOffset + BootSector->FATLength) * SectorSize);
			file.write((const char*)FATcache, BootSector->FATLength * SectorSize);
		}

		if (bitmapEntry1)
		{
			WriteClusterChain(bitmapEntry1->Cluster, AllocationBitmap, bitmapEntry1->Size);
		}

		if (bitmapEntry2)
		{
			WriteClusterChain(bitmapEntry2->Cluster, AllocationBitmap, bitmapEntry2->Size);
		}

		delete[] FATcache;
		delete[] temporaryBuffer;
		delete[] temporaryBuffer2;
		delete BootSector;
	}

	uint32_t exFATDriver::ReadFAT(uint32_t cluster)
	{
		if (cluster < 2 || cluster > TotalClusters)
		{
			return -1;
		}

		uint32_t* FATtable = (uint32_t*)FATcache;
		return FATtable[cluster];
	}

	uint32_t exFATDriver::WriteFAT(uint32_t cluster, uint32_t value)
	{
		if (cluster < 2 || cluster > TotalClusters)
		{
			return -1;
		}

		uint32_t* FATtable = (uint32_t*)FATcache;
		FATtable[cluster] = value;
		return 0;
	}

	uint32_t exFATDriver::ReadCluster(uint32_t cluster, void* buffer)
	{
		if (cluster < 2 || cluster > TotalClusters)
		{
			return -1;
		}

		uint32_t start_sector = (cluster - 2) * SectorsPerCluster + BootSector->ClusterHeapOffset;
		file.seekg(start_sector * SectorSize);
		file.read((char*)buffer, ClusterSize);
		return 0;
	}

	uint32_t exFATDriver::WriteCluster(uint32_t cluster, void* buffer)
	{
		if (cluster < 2 || cluster > TotalClusters)
		{
			return -1;
		}

		uint32_t start_sector = (cluster - 2) * SectorsPerCluster + BootSector->ClusterHeapOffset;
		file.seekg(start_sector * SectorSize);
		file.write((char*)buffer, ClusterSize);
		return 0;
	}

	std::vector<uint32_t> exFATDriver::GetClusterChain(uint32_t start)
	{
		if (start < 2 || start > TotalClusters)
		{
			return {};
		}

		std::vector<uint32_t> chain = { start };

		uint32_t current = start;
		while (true)
		{
			current = ReadFAT(current);
			if (current == BAD_CLUSTER)
			{
				return {};
			}

			if (current < END_CLUSTER)
			{
				chain.push_back(current);
			}
			else
			{
				break;
			}
		}

		return chain;
	}

	uint32_t exFATDriver::AllocateClusterChain(uint32_t size)
	{
		uint32_t totalAllocated = 0;

		uint32_t cluster = 2;
		uint32_t prevCluster = cluster;
		uint32_t firstCluster = 0;
		uint32_t clusterStatus = FREE_CLUSTER;

		while (totalAllocated < size)
		{
			if (cluster >= TotalClusters)
			{
				return BAD_CLUSTER;
			}

			clusterStatus = ReadFAT(cluster);
			if (clusterStatus == FREE_CLUSTER)
			{
				if (totalAllocated != 0)
				{
					if (WriteFAT(prevCluster, cluster) != 0)
					{
						return BAD_CLUSTER;
					}
				}
				else
				{
					firstCluster = cluster;
				}

				if (totalAllocated == (size - 1))
				{
					if (WriteFAT(cluster, END_CLUSTER) != 0)
					{
						return BAD_CLUSTER;
					}
				}

				totalAllocated++;
				prevCluster = cluster;
			}

			cluster++;
		}

		return firstCluster;
	}

	void exFATDriver::FreeClusterChain(uint32_t start)
	{
		std::vector<uint32_t> chain = GetClusterChain(start);

		for (uint32_t i = 0; i < chain.size(); i++)
		{
			WriteFAT(chain[i], FREE_CLUSTER);
		}
	}

	void* exFATDriver::ReadClusterChain(uint32_t start, uint32_t& size)
	{
		if (start < 2 || start > TotalClusters)
		{
			return {};
		}

		std::vector<uint32_t> chain = GetClusterChain(start);
		uint32_t size_ = chain.size();
		size = size_ * ClusterSize;

		uint8_t* buffer = new uint8_t[size];

		for (uint32_t i = 0; i < size; i++)
		{
			ReadCluster(chain[i], buffer);
			buffer += ClusterSize;
		}

		return (void*)buffer;
	}

	void exFATDriver::WriteClusterChain(uint32_t start, void* buffer, uint32_t size)
	{
		std::vector<uint32_t> chain = GetClusterChain(start);
		uint32_t totalSize = chain.size();
		uint32_t writeSize = size;
		if (writeSize > totalSize)
		{
			writeSize = totalSize;
		}

		uint8_t* buf = (uint8_t*)buffer;
		for (uint32_t i = 0; i < writeSize; i++)
		{
			WriteCluster(chain[i], buf);
			buf += ClusterSize;
		}
	}

	void exFATDriver::ResizeClusterChain(uint32_t start, uint32_t new_size)
	{
		std::vector<uint32_t> chain = GetClusterChain(start);
		uint32_t cur_size = chain.size();

		if (cur_size == new_size)
		{
			return;
		}
		else if (cur_size > new_size)
		{
			WriteFAT(chain[new_size - 1], END_CLUSTER);
			FreeClusterChain(chain[new_size]);
		}
		else
		{
			uint32_t start_of_the_rest = AllocateClusterChain(new_size - cur_size);
			WriteFAT(chain[cur_size - 1], start_of_the_rest);
		}
	}

	void exFATDriver::GetDirectoriesOnCluster(uint32_t cluster, std::vector<DirEntry>& entries)
	{
		if (cluster < 2 || cluster > TotalClusters)
		{
			return;
		}

		ReadCluster(cluster, temporaryBuffer);

		FileEntryGeneral* metadata = (FileEntryGeneral*)temporaryBuffer;
		uint32_t meta_pointer_iterator = 0;

		DirEntry nextFile;
		memset(&nextFile, 0, sizeof(DirEntry));

		uint32_t currentNameLength = 0;
		uint32_t nameLength = 0;
		uint32_t secondaryEntries = 0;
		uint32_t secondaryEntryCount = 0;

		while (true)
		{
			if (metadata->EntryType == ENTRY_END)
			{
				break;
			}
			else if (metadata->EntryType == ENTRY_ALLOCATION_BITMAP)
			{
				BitmapEntry* bitmapEntry = (BitmapEntry*)metadata;

				if ((bitmapEntry->BitmapNumber == 0) && (bitmapEntry1 == nullptr))
				{
					bitmapEntry1 = new BitmapEntry();
					memcpy(bitmapEntry1, bitmapEntry, sizeof(BitmapEntry));

					if ((BootSector->Flags & EX_FAT_USE_SECOND_FAT) == 0)
					{
						std::vector<uint32_t> bitmapChain = GetClusterChain(bitmapEntry1->Cluster);

						AllocationBitmap = new uint8_t[bitmapEntry1->Size];
						memset(AllocationBitmap, 0, bitmapEntry1->Size);
						uint8_t* ptr = (uint8_t*)AllocationBitmap;
						for (auto& clus : bitmapChain)
						{
							ReadCluster(clus, ptr);
							ptr += ClusterSize;
						}
					}
				}
				else if (bitmapEntry2 == nullptr)
				{
					bitmapEntry2 = new BitmapEntry();
					memcpy(bitmapEntry2, bitmapEntry, sizeof(BitmapEntry));

					if ((BootSector->Flags & EX_FAT_USE_SECOND_FAT) == EX_FAT_USE_SECOND_FAT)
					{
						std::vector<uint32_t> bitmapChain = GetClusterChain(bitmapEntry2->Cluster);

						AllocationBitmap = new uint8_t[bitmapEntry2->Size];
						memset(AllocationBitmap, 0, bitmapEntry2->Size);
						uint8_t* ptr = (uint8_t*)AllocationBitmap;
						for (auto& clus : bitmapChain)
						{
							ReadCluster(clus, ptr);
							ptr += ClusterSize;
						}
					}
				}
			}
			else if (metadata->EntryType == ENTRY_VOLUME_LABEL)
			{
				VolumeLabelEntry* volumeEntry = (VolumeLabelEntry*)metadata;
				
				for (uint32_t i = 0; i < volumeEntry->Size; i++)
				{
					VolumeLabel[i] = (char)volumeEntry->Label[i];
				}
			}
			else if (metadata->EntryType == ENTRY_FILE)
			{
				FileEntry* fileEntry = (FileEntry*)metadata;
				secondaryEntryCount = fileEntry->SecondaryEntries;

				currentNameLength = 0;
				nameLength = 0;
				secondaryEntries = 0;

				nextFile.attributes = fileEntry->FileAttributes;
				nextFile.parentCluster = cluster;
				nextFile.offsetInParentCluster = meta_pointer_iterator;
			}
			else if (metadata->EntryType == ENTRY_STREAM)
			{
				StreamEntry* streamEntry = (StreamEntry*)metadata;
				nameLength = streamEntry->NameLength;

				nextFile.size = streamEntry->DataLength;
				nextFile.cluster = streamEntry->FirstCluster;

				secondaryEntries++;
			}
			else if (metadata->EntryType == ENTRY_FILENAME)
			{
				uint32_t toCopy = (nameLength - currentNameLength) % 16;

				FileNameEntry* nameEntry = (FileNameEntry*)metadata;
				for (uint32_t i = 0; i < toCopy; i++)
				{
					nextFile.name[currentNameLength + i] = (char)nameEntry->FileName[i];
				}

				currentNameLength += toCopy;
				secondaryEntries++;

				if (secondaryEntries == secondaryEntryCount)
				{
					entries.push_back(nextFile);
					memset(&nextFile, 0, sizeof(DirEntry));
				}
			}

			metadata++;
			meta_pointer_iterator++;
		}
	}

	uint32_t exFATDriver::GetClusterFromFilePath(const char* filePath, DirEntry* entry)
	{
		char fileNamePart[256];
		uint16_t start = 2; //Skip the "~/" part
		uint32_t active_cluster = BootSector->RootDirectoryCluster;
		DirEntry fileInfo;

		uint32_t iterator = 2;
		if (strcmp(filePath, "~") == 0)
		{
			fileInfo.attributes = FILE_DIRECTORY | FILE_VOLUME_ID;
			fileInfo.size = 0;
			fileInfo.cluster = active_cluster;
		}
		else
		{
			for (iterator = 2; filePath[iterator - 1] != 0; iterator++)
			{
				if (filePath[iterator] == '/' || filePath[iterator] == 0)
				{
					memset(fileNamePart, 0, 256);
					memcpy(fileNamePart, (void*)((uint64_t)filePath + start), iterator - start);

					int retVal = DirectorySearch(fileNamePart, active_cluster, &fileInfo);

					if (retVal != 0)
					{
						return retVal;
					}

					start = iterator + 1;
					active_cluster = fileInfo.cluster;
				}
			}
		}

		if (entry)
		{
			*entry = fileInfo;
		}

		return active_cluster;
	}

	std::vector<DirEntry> exFATDriver::GetDirectories(uint32_t cluster, uint32_t filter_attributes, bool exclude)
	{
		std::vector<DirEntry> ret;
		if (cluster < 2 || cluster > TotalClusters)
		{
			return ret;
		}

		std::vector<DirEntry> all;
		GetDirectoriesOnCluster(cluster, all);

		for (auto elem : all)
		{
			if (!exclude)
			{
				if (elem.attributes & filter_attributes)
				{
					continue;
				}

				ret.push_back(elem);
			}
			else
			{
				if (elem.attributes & filter_attributes)
				{
					ret.push_back(elem);
				}
			}
		}

		return ret;
	}

	void exFATDriver::ModifyDirectoryEntry(uint32_t cluster, const char* name, DirEntry modified)
	{
	}

	int exFATDriver::PrepareAddedDirectory(uint32_t cluster)
	{
		return 0;
	}

	void exFATDriver::CleanFileEntry(uint32_t cluster, DirEntry entry)
	{
		entry.attributes = 0;
		ModifyDirectoryEntry(cluster, entry.name, entry);
	}

	int exFATDriver::DirectorySearch(const char* FilePart, uint32_t cluster, DirEntry* file)
	{
		if (cluster < 2 || cluster > TotalClusters)
		{
			return -1;
		}

		std::vector<DirEntry> entries;
		GetDirectoriesOnCluster(cluster, entries);

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

	int exFATDriver::DirectoryAdd(uint32_t cluster, DirEntry file)
	{
		return 0;
	}

	int exFATDriver::OpenFile(const char* filePath, DirEntry* fileMeta)
	{
		if (fileMeta == nullptr)
		{
			return -1;
		}

		int ret = GetClusterFromFilePath(filePath, fileMeta);
		if (ret < 0)
		{
			return ret;
		}

		return 0;
	}

	int exFATDriver::CreateFile(const char* filePath, DirEntry* fileMeta)
	{
		DirEntry parentInfo;
		uint32_t active_cluster = GetClusterFromFilePath(filePath, &parentInfo);

		fileMeta->parentCluster = active_cluster;
		fileMeta->offsetInParentCluster = -1;

		//Makes sure there's no other file like this
		int retVal = DirectorySearch(fileMeta->name, active_cluster, nullptr);
		if (retVal != -2)
		{
			return retVal;
		}

		if ((parentInfo.attributes & FILE_DIRECTORY) != FILE_DIRECTORY)
		{
			return -2;
		}

		retVal = DirectoryAdd(active_cluster, *fileMeta);
		if (retVal != 0)
		{
			return -1;
		}

		return DirectorySearch(fileMeta->name, active_cluster, fileMeta);
	}

	int exFATDriver::DeleteFile(DirEntry entry)
	{
		if ((entry.attributes & FILE_DIRECTORY) == FILE_DIRECTORY)
		{
			std::vector<DirEntry> subDirs = GetDirectories(entry.cluster, 0, false);

			for (const auto& dir : subDirs)
			{
				DeleteFile(dir);
			}
		}

		FreeClusterChain(entry.cluster);
		CleanFileEntry(entry.parentCluster, entry);

		return 0;
	}

	int exFATDriver::ReadFile(DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes)
	{
		if ((fileMeta.attributes & FILE_DIRECTORY) == FILE_DIRECTORY)
		{
			return -3;
		}

		if ((bytes + offset) > fileMeta.size)
		{
			bytes = fileMeta.size - offset;
		}

		std::vector<uint32_t> chain = GetClusterChain(fileMeta.cluster);

		uint64_t bytes_so_far = 0;
		uint8_t* buff = (uint8_t*)buffer;
		for (uint32_t i = 0; i < chain.size(); i++)
		{
			uint32_t clus = chain[i];

			if (bytes_so_far < offset)
			{
				bytes_so_far += ClusterSize;
				continue;
			}

			uint64_t toRead = (bytes + offset) - bytes_so_far;
			if (toRead > ClusterSize)
			{
				toRead = ClusterSize;
				ReadCluster(clus, buff);
			}
			else
			{
				uint8_t* temporary = new uint8_t[ClusterSize];
				ReadCluster(clus, temporary);
				memcpy(buff, temporary, toRead);
				delete[] temporary;
			}

			buff += ClusterSize;
			bytes_so_far += toRead;
		}

		return 0;
	}

	int exFATDriver::WriteFile(DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes)
	{
		if ((fileMeta.attributes & FILE_DIRECTORY) == FILE_DIRECTORY)
		{
			return -3;
		}

		if ((bytes + offset) > fileMeta.size)
		{
			fileMeta.size = bytes + offset;

			uint32_t new_cluster_size = fileMeta.size / ClusterSize;
			if ((new_cluster_size * ClusterSize) != fileMeta.size)
			{
				new_cluster_size++;
			}

			ResizeClusterChain(fileMeta.cluster, new_cluster_size);
		}

		std::vector<uint32_t> chain = GetClusterChain(fileMeta.cluster);

		uint64_t bytes_so_far = 0;
		uint8_t* buff = (uint8_t*)buffer;
		for (uint32_t i = 0; i < chain.size(); i++)
		{
			uint32_t clus = chain[i];

			if (bytes_so_far < offset)
			{
				bytes_so_far += ClusterSize;
				continue;
			}

			uint64_t toWrite = (bytes + offset) - bytes_so_far;
			if (toWrite > ClusterSize)
			{
				toWrite = ClusterSize;
				WriteCluster(clus, buff);
			}
			else
			{
				uint8_t* temporary = new uint8_t[ClusterSize];
				memset(temporary, 0, ClusterSize);
				memcpy(temporary, buff, toWrite);
				WriteCluster(clus, temporary);
				delete[] temporary;
			}

			buff += ClusterSize;
			bytes_so_far += toWrite;
		}

		ModifyDirectoryEntry(fileMeta.parentCluster, fileMeta.name, fileMeta);
		return 0;
	}

	int exFATDriver::ResizeFile(DirEntry fileMeta, uint32_t new_size)
	{
		if ((fileMeta.attributes & FILE_DIRECTORY) == FILE_DIRECTORY)
		{
			return -3;
		}

		fileMeta.size = new_size;

		uint32_t new_cluster_size = fileMeta.size / ClusterSize;
		if ((new_cluster_size * ClusterSize) != fileMeta.size)
		{
			new_cluster_size++;
		}

		ResizeClusterChain(fileMeta.cluster, new_cluster_size);
		ModifyDirectoryEntry(fileMeta.parentCluster, fileMeta.name, fileMeta);

		return 0;
	}
};
