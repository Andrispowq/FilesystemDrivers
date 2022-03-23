#include "FAT32Driver.h"

FAT32Driver::FAT32Driver(const std::string& image)
{
	file.open(image, std::ios::in | std::ios::out | std::ios::binary);

	if (!file.is_open())
	{
		std::cerr << "ERROR: " << image << " could not be opened!\n";
	}

	BootSector = new FAT32_BootSector();
	file.read((char*)BootSector, sizeof(FAT32_BootSector));

 	FirstDataSector = BootSector->NumberOfFATs * BootSector->SectorsPerFAT32 + BootSector->ReservedSectors;
	BootSector->Reserved0 = FirstDataSector;

	RootDirStart = BootSector->RootDirStart;
	ClusterSize = BootSector->SectorsPerCluster * BootSector->BytesPerSector;

	if (BootSector->TotalSectors == 0)
	{
		TotalSectors = BootSector->LargeTotalSectors;
	}
	else
	{
		TotalSectors = BootSector->TotalSectors;
	}

	TotalClusters = TotalSectors / BootSector->SectorsPerCluster;

	temporaryBuffer = new uint8_t[ClusterSize];
	temporaryBuffer2 = new uint8_t[ClusterSize];

	FATcache = new uint8_t[BootSector->SectorsPerFAT32 * BootSector->BytesPerSector];
	file.seekg(BootSector->ReservedSectors * BootSector->BytesPerSector);
	file.read((char*)FATcache, BootSector->SectorsPerFAT32 * BootSector->BytesPerSector);
}

FAT32Driver::~FAT32Driver()
{
	file.seekg(0);
	file.write((const char*)BootSector, 512);
	file.seekg(BootSector->BackupBootSector * BootSector->BytesPerSector);
	file.write((const char*)BootSector, 512);
	
	file.seekg(BootSector->ReservedSectors * BootSector->BytesPerSector);
	file.write((const char*)FATcache, BootSector->SectorsPerFAT32 * BootSector->BytesPerSector);

	//copy the first FAT onto the other ones upon closing the FS
	for (uint32_t i = 1; i < BootSector->NumberOfFATs; i++)
	{
		file.seekg(BootSector->ReservedSectors * BootSector->BytesPerSector + BootSector->SectorsPerFAT32 * BootSector->BytesPerSector * i);
		file.write((const char*)FATcache, BootSector->SectorsPerFAT32 * BootSector->BytesPerSector);
	}

	delete[] FATcache;
	delete[] temporaryBuffer;
	delete[] temporaryBuffer2;
	delete BootSector;
}

uint32_t FAT32Driver::ReadFAT(uint32_t cluster)
{
	if (cluster < 2 || cluster > TotalClusters)
	{
		return -1;
	}

	uint32_t* FATtable = (uint32_t*)FATcache;
	return FATtable[cluster] & 0x0FFFFFFF;
}

uint32_t FAT32Driver::WriteFAT(uint32_t cluster, uint32_t value)
{
	if (cluster < 2 || cluster > TotalClusters)
	{
		return -1;
	}

	uint32_t* FATtable = (uint32_t*)FATcache;
	FATtable[cluster] &= 0xF0000000;
	FATtable[cluster] |= (value & 0x0FFFFFFF);
	return 0;
}

uint32_t FAT32Driver::ReadCluster(uint32_t cluster, void* buffer)
{
	if (cluster < 2 || cluster > TotalClusters)
	{
		return -1;
	}

	uint32_t start_sector = (cluster - 2) * BootSector->SectorsPerCluster + FirstDataSector;
	file.seekg(start_sector * BootSector->BytesPerSector);
	file.read((char*)buffer, ClusterSize);
	return 0;
}

uint32_t FAT32Driver::WriteCluster(uint32_t cluster, void* buffer)
{
	if (cluster < 2 || cluster > TotalClusters)
	{
		return -1;
	}

	uint32_t start_sector = (cluster - 2) * BootSector->SectorsPerCluster + FirstDataSector;
	file.seekg(start_sector * BootSector->BytesPerSector);
	file.write((char*)buffer, ClusterSize);
	return 0;
}

std::vector<uint32_t> FAT32Driver::GetClusterChain(uint32_t start)
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

uint32_t FAT32Driver::AllocateClusterChain(uint32_t size)
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

void FAT32Driver::FreeClusterChain(uint32_t start)
{
	std::vector<uint32_t> chain = GetClusterChain(start);

	for (uint32_t i = 0; i < chain.size(); i++)
	{
		WriteFAT(chain[i], FREE_CLUSTER);
	}
}

void* FAT32Driver::ReadClusterChain(uint32_t start, uint32_t& size)
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

void FAT32Driver::WriteClusterChain(uint32_t start, void* buffer, uint32_t size)
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

void FAT32Driver::ResizeClusterChain(uint32_t start, uint32_t new_size)
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

void FAT32Driver::GetDirectoriesOnCluster(uint32_t cluster, std::vector<DirEntry>& entries)
{
	if (cluster < 2 || cluster > TotalClusters)
	{
		return;
	}

	ReadCluster(cluster, temporaryBuffer);

	DirectoryEntry* metadata = (DirectoryEntry*)temporaryBuffer;
	uint32_t meta_pointer_iterator = 0;

	bool LFN = false;
	while (true)
	{
		if (metadata->name[0] == ENTRY_END)
		{
			break;
		}
		else if ((metadata->name[0] == (char)ENTRY_FREE) || ((metadata->attributes & FILE_LONG_NAME) == FILE_LONG_NAME))
		{
			LFN = ((metadata->attributes & FILE_LONG_NAME) == FILE_LONG_NAME);

			//If we are under the cluster limit
			if (meta_pointer_iterator < ClusterSize / sizeof(DirectoryEntry) - 1)
			{
				metadata++;
				meta_pointer_iterator++;
			}
			//Search next cluster
			else
			{
				uint32_t next_cluster = ReadFAT(cluster);
				if (next_cluster >= END_CLUSTER)
				{
					break;
				}
				else
				{
					//Search next cluster
					return GetDirectoriesOnCluster(next_cluster, entries);
				}
			}
		}
		else
		{
			DirEntry entry = FromFATEntry(metadata, LFN);
			entry.parentCluster = cluster;
			entry.offsetInParentCluster = meta_pointer_iterator;
			entries.push_back(entry);

			LFN = false;

			metadata++;
			meta_pointer_iterator++;
		}
	}
}

std::vector<DirEntry> FAT32Driver::GetDirectories(uint32_t cluster, uint32_t filter_attributes, bool exclude)
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

void FAT32Driver::ModifyDirectoryEntry(uint32_t cluster, const char* name, DirEntry modified)
{
	bool isLFN = false;
	if (IsFATFormat((char*)name) != 0)
	{
		isLFN = true;
	}

	ReadCluster(cluster, temporaryBuffer);

	DirectoryEntry* metadata = (DirectoryEntry*)temporaryBuffer;
	uint32_t meta_pointer_iterator = 0;

	uint32_t count = 0;
	DirectoryEntry* ent = ToFATEntry(modified, count);

	while (1)
	{
		if (metadata->name[0] == ENTRY_END)
		{
			break;
		}
		else if (((metadata->attributes & FILE_LONG_NAME) == FILE_LONG_NAME) || !(Compare(metadata, name, isLFN)))
		{
			//If we are under the cluster limit
			if (meta_pointer_iterator < ClusterSize / sizeof(DirectoryEntry) - 1)
			{
				metadata++;
				meta_pointer_iterator++;
			}
			//Search next cluster
			else
			{
				uint32_t next_cluster = ReadFAT(cluster);
				if (next_cluster >= END_CLUSTER)
				{
					break;
				}
				else
				{
					//Search next cluster
					return ModifyDirectoryEntry(next_cluster, name, modified);
				}
			}
		}
		else
		{
			if (modified.attributes == 0)
			{
				//We want to delete the entry altogether
				for (uint32_t i = 0; i < (count + 1); i++)
				{
					DirectoryEntry* curr = metadata - i;
					curr->name[0] = ENTRY_FREE;
				}
			}
			else
			{
				metadata->attributes = modified.attributes;
				metadata->clusterLow = modified.cluster & 0xFFFF;
				metadata->clusterHigh = (modified.cluster << 16) & 0xFFFF;
				metadata->fileSize = modified.size;
			}

			WriteCluster(cluster, temporaryBuffer);
			break;
		}
	}
}

int FAT32Driver::PrepareAddedDirectory(uint32_t cluster)
{
	if (cluster < 2 || cluster > TotalClusters)
	{
		return -1;
	}

	char* tempBuff = new char[ClusterSize];
	ReadCluster(cluster, tempBuff);

	DirectoryEntry* metadata = (DirectoryEntry*)tempBuff;
	memset(metadata, 0, sizeof(DirectoryEntry));
	memcpy(metadata->name, ".          ", 11);
	metadata->attributes = FILE_DIRECTORY;
	metadata->clusterLow = cluster & 0xFFFF;
	metadata->clusterHigh = (cluster >> 16) & 0xFFFF;

	metadata->ctime_date = GetDate();
	metadata->ctime_time = GetTime();
	metadata->ctime_ms = GetMilliseconds();
	metadata->atime_date = GetDate();
	metadata->mtime_date = GetDate();
	metadata->mtime_time = GetTime();

	metadata++;
	memset(metadata, 0, sizeof(DirectoryEntry));
	memcpy(metadata->name, "..         ", 11);
	metadata->attributes = FILE_DIRECTORY;

	metadata->ctime_date = GetDate();
	metadata->ctime_time = GetTime();
	metadata->ctime_ms = GetMilliseconds();
	metadata->atime_date = GetDate();
	metadata->mtime_date = GetDate();
	metadata->mtime_time = GetTime();

	WriteCluster(cluster, tempBuff);
	delete[] tempBuff;

	return 0;
}

void FAT32Driver::CleanFileEntry(uint32_t cluster, DirEntry entry)
{
	entry.attributes = 0;
	ModifyDirectoryEntry(cluster, entry.name, entry);
}

int FAT32Driver::DirectorySearch(const char* FilePart, uint32_t cluster, DirEntry* file)
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

int FAT32Driver::DirectoryAdd(uint32_t cluster, DirEntry file)
{
	if (cluster < 2 || cluster > TotalClusters)
	{
		return -1;
	}

	bool isLFN = false;
	if (IsFATFormat(file.name) != 0)
	{
		isLFN = true;
	}

	ReadCluster(cluster, temporaryBuffer);

	DirectoryEntry* metadata = (DirectoryEntry*)temporaryBuffer;
	uint32_t meta_pointer_iterator = 0;

	uint32_t count;
	DirectoryEntry* ent = ToFATEntry(file, count);

	uint32_t freeCount = 0;
	while (1)
	{
		if (((metadata->name[0] != (char)ENTRY_FREE) && (metadata->name[0] != ENTRY_END)))
		{
			if (meta_pointer_iterator < ClusterSize / sizeof(DirectoryEntry) - 1)
			{
				metadata++;
				meta_pointer_iterator++;
			}
			else
			{
				uint32_t next_cluster = ReadFAT(cluster);
				if (next_cluster >= END_CLUSTER)
				{
					next_cluster = AllocateClusterChain(1);
					if (next_cluster == BAD_CLUSTER)
					{
						return -1;
					}

					WriteFAT(cluster, next_cluster);
				}

				return DirectoryAdd(next_cluster, file);
			}
		}
		else
		{
			if (freeCount < count)
			{
				freeCount++;

				if (meta_pointer_iterator < ClusterSize / sizeof(DirectoryEntry) - 1)
				{
					metadata++;
					meta_pointer_iterator++;
					continue;
				}
				else
				{
					uint32_t next_cluster = ReadFAT(cluster);
					if (next_cluster >= END_CLUSTER)
					{
						next_cluster = AllocateClusterChain(1);
						if (next_cluster == BAD_CLUSTER)
						{
							return -1;
						}

						WriteFAT(cluster, next_cluster);
					}

					return DirectoryAdd(next_cluster, file);
				}
			}
			
			ent->ctime_date = GetDate();
			ent->ctime_time = GetTime();
			ent->ctime_ms = GetMilliseconds();
			ent->atime_date = GetDate();
			ent->mtime_date = GetDate();
			ent->mtime_time = GetTime();

			//For now we assume that the long entries fit in one cluster, let's hope it's true
			uint32_t clust_size = ent->fileSize / ClusterSize;
			if ((clust_size * ClusterSize) != ClusterSize)
			{
				clust_size++;
			}

			uint32_t new_cluster = AllocateClusterChain(clust_size);
			if (new_cluster == BAD_CLUSTER)
			{
				return -1;
			}

			char* buffer = new char[clust_size * ClusterSize];
			memset(buffer, 0, clust_size * ClusterSize);
			WriteClusterChain(new_cluster, buffer, clust_size * ClusterSize);

			if ((ent->attributes & FILE_DIRECTORY) == FILE_DIRECTORY)
			{
				PrepareAddedDirectory(new_cluster);
			}

			ent->clusterLow = new_cluster & 0xFFFF;
			ent->clusterHigh = (new_cluster >> 16) & 0xFFFF;

			memcpy(metadata - count, ent - count, sizeof(DirectoryEntry) * (count + 1));
			WriteCluster(cluster, temporaryBuffer); //Write the modified stuff back

			return 0;
		}
	}
	return -1;
}

int FAT32Driver::OpenFile(const char* filePath, DirEntry* fileMeta)
{
	if (fileMeta == nullptr)
	{
		return -1;
	}

	GetClusterFromFilePath(filePath, fileMeta);
	return 0;
}

int FAT32Driver::CreateFile(const char* filePath, DirEntry* fileMeta)
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

	DirectoryAdd(active_cluster, *fileMeta);
	return DirectorySearch(fileMeta->name, active_cluster, fileMeta);
}

int FAT32Driver::DeleteFile(DirEntry entry)
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

int FAT32Driver::ReadFile(DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes)
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

int FAT32Driver::WriteFile(DirEntry fileMeta, uint64_t offset, void* buffer, uint64_t bytes)
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

int FAT32Driver::ResizeFile(DirEntry fileMeta, uint32_t new_size)
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

uint32_t FAT32Driver::GetClusterFromFilePath(const char* filePath, DirEntry* entry)
{
	char fileNamePart[256];
	uint16_t start = 2; //Skip the "~/" part
	uint32_t active_cluster = RootDirStart;
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

DirEntry FAT32Driver::FromFATEntry(DirectoryEntry* entry, bool long_fname)
{
	DirEntry ent;

	if (long_fname)
	{
		char long_name[128];
		uint32_t count = 0;
		LongDirectoryEntry* lEntry = (LongDirectoryEntry*)(entry - 1);
		while (true)
		{
			for (uint32_t i = 0; i < 5; i++)
			{
				long_name[count++] = (char)lEntry->name0[i];
			}
			for (uint32_t i = 0; i < 6; i++)
			{
				long_name[count++] = (char)lEntry->name1[i];
			}
			for (uint32_t i = 0; i < 2; i++)
			{
				long_name[count++] = (char)lEntry->name2[i];
			}

			if ((lEntry->order & FILE_LAST_LONG_ENTRY) == FILE_LAST_LONG_ENTRY)
			{
				break;
			}

			lEntry--;
		}

		memcpy(ent.name, long_name, count);
		ent.name[count] = 0;
	}
	else
	{
		char out[13];
		ConvertFromFATFormat(entry->name, out);
		strcpy(ent.name, out);

		for (uint32_t i = 11; i >= 0; i--)
		{
			if (ent.name[i] == ' ')
			{
				ent.name[i] = 0;
			}
			else
			{
				break;
			}
		}
	}

	ent.size = entry->fileSize;
	ent.attributes = entry->attributes;
	ent.cluster = entry->clusterLow | (entry->clusterHigh << 16);

	return ent;
}

DirectoryEntry* FAT32Driver::ToFATEntry(DirEntry entry, uint32_t& longEntries)
{
	char* namePtr = entry.name;
	size_t nameLen = strlen(namePtr);
	if (nameLen <= 12)
	{
		char* fat_name = nullptr;
		if ((strncmp(namePtr, ".          ", 11) == 0) || (strncmp(namePtr, "..         ", 11) == 0))
		{
			fat_name = namePtr;
		}
		else
		{
			fat_name = ConvertToFATFormat(namePtr);
		}

		longEntries = 0;

		DirectoryEntry* ent = new DirectoryEntry();
		memset(ent, 0, sizeof(DirectoryEntry));
		memcpy(ent->name, fat_name, 11);
		ent->attributes = entry.attributes;
		ent->fileSize = entry.size;
		ent->clusterLow = entry.cluster & 0xFFFF;
		ent->clusterHigh = (entry.cluster >> 16) & 0xFFFF;
		return ent;

		delete[] fat_name;
	}
	else
	{
		size_t long_entries = (nameLen / 13) + 1;
		longEntries = (uint32_t)long_entries;
		LongDirectoryEntry* entries = new LongDirectoryEntry[long_entries + 1];
		LongDirectoryEntry* start = entries + (long_entries - 1);

		char shortName[11];
		for (uint32_t i = 0; i < 6; i++)
		{
			shortName[i] = std::toupper(namePtr[i]);
		}

		shortName[6] = '~';
		shortName[7] = '1';

		bool hasEXT = false;
		for (uint32_t i = 6; i < nameLen; i++)
		{
			if (namePtr[i] == '.')
			{
				hasEXT = true;
				for (uint32_t j = 0; (j < (nameLen - (i + 1))) && (j < 3); j++)
				{
					shortName[j + 8] = std::toupper(namePtr[i + 1 + j]);
				}
				break;
			}
		}

		if (!hasEXT)
		{
			shortName[8] = ' ';
			shortName[9] = ' ';
			shortName[10] = ' ';
		}

		uint8_t checksum = 0;

		for (size_t i = 11; i; i--)
		{
			checksum = ((checksum & 1) << 7) + (checksum >> 1) + shortName[11 - i];
		}

		uint32_t counter = 0;
		for (size_t j = 0; j < long_entries; j++)
		{
			memset(start, 0, sizeof(LongDirectoryEntry));

			start->order = uint8_t(j + 1);
			if (j == (long_entries - 1))
			{
				start->order |= 0x40;
			}

			start->attributes = FILE_LONG_NAME;
			start->checksum = checksum;

			for (uint32_t i = 0; i < 5; i++)
			{
				if (counter >= nameLen)
				{
					start->name0[i] = 0;
					break;
				}

				start->name0[i] = (uint16_t)namePtr[counter++];
			}
			for (uint32_t i = 0; i < 6; i++)
			{
				if (counter >= nameLen)
				{
					start->name1[i] = 0;
					break;
				}

				start->name1[i] = (uint16_t)namePtr[counter++];
			}
			for (uint32_t i = 0; i < 2; i++)
			{
				if (counter >= nameLen)
				{
					start->name2[i] = 0;
					break;
				}

				start->name2[i] = (uint16_t)namePtr[counter++];
			}

			start--;
		}

		DirectoryEntry* dirEntry = (DirectoryEntry*)(entries + long_entries);
		memset(dirEntry, 0, sizeof(DirectoryEntry));
		memcpy(dirEntry->name, shortName, 11);
		dirEntry->attributes = entry.attributes;
		dirEntry->fileSize = entry.size;
		dirEntry->clusterLow = entry.cluster & 0xFFFF;
		dirEntry->clusterHigh = (entry.cluster >> 16) & 0xFFFF;

		return dirEntry;
	}

	return nullptr;
}

bool FAT32Driver::Compare(DirectoryEntry* entry, const char* name, bool long_name)
{
	DirEntry ent = FromFATEntry(entry, long_name);
	return strcmp(ent.name, name) == 0;
}

uint8_t FAT32Driver::GetMilliseconds() const
{
	return 0; //Currently not implemented
}

uint16_t FAT32Driver::GetTime() const
{
	time_t rawtime;
	struct tm* timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	uint16_t sec_over_2 = timeinfo->tm_sec / 2;
	uint16_t min = timeinfo->tm_min;
	uint16_t hour = timeinfo->tm_hour;

	return sec_over_2 | (min << 5) | (hour << 11);
}

uint16_t FAT32Driver::GetDate() const
{
	time_t rawtime;
	struct tm* timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	uint16_t day = timeinfo->tm_mday;
	uint16_t mon = timeinfo->tm_mon + 1;
	uint16_t year = timeinfo->tm_year + 1900;

	return day | (mon << 5) | ((year - 1980) << 9);
}

int FAT32Driver::IsFATFormat(char* name)
{
	size_t len = strlen(name);
	if (len != 11)
	{
		return NOT_CONVERTED_YET;
	}

	if ((strncmp(name, ".          ", 11) == 0) || (strncmp(name, "..         ", 11) == 0))
	{
		return 0;
	}

	int retVal = 0;

	uint32_t iterator;
	for (iterator = 0; iterator < 11; iterator++)
	{
		if (name[iterator] < 0x20 && name[iterator] != 0x05)
		{
			retVal = retVal | BAD_CHARACTER;
		}

		switch (name[iterator])
		{
		case 0x2E:
		case 0x22:
		case 0x2A:
		case 0x2B:
		case 0x2C:
		case 0x2F:
		case 0x3A:
		case 0x3B:
		case 0x3C:
		case 0x3D:
		case 0x3E:
		case 0x3F:
		case 0x5B:
		case 0x5C:
		case 0x5D:
		case 0x7C:
			retVal = retVal | BAD_CHARACTER;
		}

		if (name[iterator] >= 'a' && name[iterator] <= 'z')
		{
			retVal = retVal | LOWERCASE_ISSUE;
		}
	}

	return retVal;
}

char* FAT32Driver::ConvertToFATFormat(char* input)
{
	uint32_t counter = 0;

	for (uint32_t i = 0; i < 12; i++)
	{
		input[i] = std::toupper(input[i]);
	}

	char searchName[13] = { '\0' };
	uint16_t dotPos = 0;
	bool hasEXT = false;

	counter = 0;
	while (counter <= 8) //copy all the characters from filepart into searchname until a dot or null character is encountered
	{
		if (input[counter] == '.' || input[counter] == '\0')
		{
			dotPos = counter;

			if (input[counter] == '.')
			{
				counter++; //iterate off dot
				hasEXT = true;
			}

			break;
		}
		else
		{
			searchName[counter] = input[counter];
			counter++;
		}
	}

	if (counter > 9) //a sanity check in case there was a dot-less 11 character filename
	{
		counter = 8;
		dotPos = 8;
	}

	uint16_t extCount = 8;
	while (extCount < 11) //add the extension to the end, putting spaces where necessary
	{
		if (input[counter] != '\0' && hasEXT)
			searchName[extCount] = input[counter];
		else
			searchName[extCount] = ' ';

		counter++;
		extCount++;
	}

	counter = dotPos; //reset counter to position of the dot

	while (counter < 8) //if the dot is within the 8 character limit of the name, iterate through searchName putting in spaces up until the extension
	{
		searchName[counter] = ' ';
		counter++;
	}

	strcpy(input, searchName); //copy results back to input

	return input;
}

void FAT32Driver::ConvertFromFATFormat(char* input, char* output)
{
	//If the entry passed in is one of the dot special entries, just return them unchanged.
	if (input[0] == '.')
	{
		if (input[1] == '.')
		{
			strcpy(output, "..");
			return;
		}

		strcpy(output, ".");
		return;
	}

	uint16_t counter = 0;

	//iterate through the 8 letter file name, adding a dot when the end is reached
	for (counter = 0; counter < 8; counter++)
	{
		if (input[counter] == 0x20)
		{
			output[counter] = '.';
			break;
		}

		output[counter] = input[counter];
	}

	//if the entire 8 letters of the file name were used, tack a dot onto the end
	if (counter == 8)
	{
		output[counter] = '.';
	}

	uint16_t counter2 = 8;

	//iterate through the three-letter extension, adding it on. (Note: if the input is a directory (which has no extension) it erases the dot put in previously)
	for (counter2 = 8; counter2 < 11; counter2++)
	{
		++counter;
		if (input[counter2] == 0x20)
		{
			if (counter2 == 8) //there is no extension, the dot added earlier must be removed
				counter -= 2; //it's minus two because the for loop above iterates the loop as well
			break;
		}

		output[counter] = input[counter2];
	}

	++counter;
	while (counter < 12)
	{
		output[counter] = ' ';
		++counter;
	}

	output[12] = '\0'; //ensures proper termination regardless of program operation previously
	return;
}
