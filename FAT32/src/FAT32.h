#ifndef FAT32_H
#define FAT32_H

#include "FAT32Driver.h"

struct FAT32_FolderStructure
{
	FAT32_FolderStructure* parent;
	DirEntry entry;

	std::vector<FAT32_FolderStructure*> children;
};

struct FAT32_OpenFile
{
	FAT32_FolderStructure* file;
	uint64_t SeekPosition = 0;
	const char* path;
};

class FAT32
{
public:
	FAT32(const std::string& file);
	~FAT32();

	FAT32_OpenFile* OpenFile(const std::string& path);
	FAT32_OpenFile* CreateFile(const std::string& path, const std::string& fileName, uint8_t attributes, uint64_t size);
	void DeleteFile(FAT32_OpenFile* file);
	void CloseFile(FAT32_OpenFile* file);

	int ReadFile(FAT32_OpenFile* file, void* buffer, uint64_t nBytes);
	int WriteFile(FAT32_OpenFile* file, void* buffer, uint64_t nBytes);
	int ResizeFile(FAT32_OpenFile* file, uint64_t new_size);

	FAT32_FolderStructure* GetRoot() const { return root; }
	FAT32_FolderStructure* GetCurrent() const { return current; }

	void AddToRecords(const std::string& path, DirEntry entry);
	void DeleteFromRecords(const std::string& name);

	std::string GetCurrentDirectory() const;

	FAT32Driver* GetDriver() { return driver; }

	void ListCurrent();
	void GoTo(char* name);

private:
	FAT32_FolderStructure* RetrieveFromRoot(DirEntry entry);

	FAT32_FolderStructure* GetFolderByCluster(uint32_t cluster);
	std::vector<FAT32_FolderStructure*> GetAllEntries(FAT32_FolderStructure* top);

	FAT32Driver* driver;

	FAT32_FolderStructure* root;
	FAT32_FolderStructure* current;
};

#endif