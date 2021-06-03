#include "FAT32.h"

FAT32::FAT32(const std::string& file)
{
	driver = new FAT32Driver(file);
	uint32_t rootDirStart = driver->GetRootDirStart();

	root = new FAT32_FolderStructure();
	root->parent = nullptr;
	strcpy(root->entry.name, "~");
	root->entry.size = 0;
	root->entry.attributes = 0;
	root->entry.cluster = rootDirStart;

	current = root;

	std::vector<uint32_t> clustersToRead = { rootDirStart };
	std::vector<uint32_t> nextClustersToRead;

	while (clustersToRead.size() != 0)
	{
		for (auto clus : clustersToRead)
		{
			//Ain't no need for the volume's identifier (a FILE_VOLUME_ID entry on root)
			std::vector<DirEntry> rootDir = driver->GetDirectories(clus, FILE_VOLUME_ID, false);

			for (size_t i = 0; i < rootDir.size(); i++)
			{
				DirEntry parent = rootDir[0];
				DirEntry entry = rootDir[i];

				if (((entry.attributes & FILE_DIRECTORY) == FILE_DIRECTORY) && (entry.name[0] != '.'))
				{
					nextClustersToRead.push_back(entry.cluster);
				}

				FAT32_FolderStructure* curr = new FAT32_FolderStructure();
				FAT32_FolderStructure* par_dir = GetFolderByCluster(parent.cluster);

				//If we don't have '.' and '..' entries, we're in the root directory
				if (parent.name[0] != '.')
				{
					curr->parent = root;
					curr->entry = entry;

					root->children.push_back(curr);
				}
				else
				{
					curr->parent = par_dir;
					curr->entry = entry;

					par_dir->children.push_back(curr);
				}
			}
		}

		clustersToRead = nextClustersToRead;
		nextClustersToRead.clear();
	}
}

FAT32::~FAT32()
{
	delete root;
	delete driver;
}

FAT32_OpenFile* FAT32::OpenFile(const std::string& path)
{
	DirEntry entry;
	int ret = driver->OpenFile(path.c_str(), &entry);

	if (ret != 0)
	{
		return nullptr;
	}

	std::string full = path;
	char* ptr = new char[full.length() + 1];
	strcpy(ptr, full.c_str());

	FAT32_OpenFile* file = new FAT32_OpenFile();
	file->file = RetrieveFromRoot(entry);
	file->path = ptr;

	if (file->file == nullptr)
	{
		delete[] file->path;
		delete file;
		return nullptr;
	}

	return file;
}

FAT32_OpenFile* FAT32::CreateFile(const std::string& path, const std::string& fileName, uint8_t attributes, uint64_t size)
{
	DirEntry entry;
	strcpy(entry.name, fileName.c_str());
	entry.cluster = 0;
	entry.attributes = attributes;
	entry.size = (uint32_t)size;
	driver->CreateFile(path.c_str(), &entry);

	AddToRecords(path, entry); //For now, we add it to the current directory

	std::string full = path + '/' + fileName;
	char* ptr = new char[full.length() + 1];
	strcpy(ptr, full.c_str());

	FAT32_OpenFile* file = new FAT32_OpenFile();
	file->file = RetrieveFromRoot(entry);
	file->path = ptr;

	return file;
}

void FAT32::DeleteFile(FAT32_OpenFile* file)
{
	driver->DeleteFile(file->file->entry);

	DeleteFromRecords(file->path);
	CloseFile(file);
}

void FAT32::CloseFile(FAT32_OpenFile* file)
{
	delete[] file->path;
	delete file;
}

int FAT32::ReadFile(FAT32_OpenFile* file, void* buffer, uint64_t nBytes)
{
	uint64_t seek_pos = file->SeekPosition;
	driver->ReadFile(file->file->entry, seek_pos, buffer, nBytes);
	file->SeekPosition += nBytes;

	return 0;
}

int FAT32::WriteFile(FAT32_OpenFile* file, void* buffer, uint64_t nBytes)
{
	uint64_t seek_pos = file->SeekPosition;
	driver->WriteFile(file->file->entry, seek_pos, buffer, nBytes);
	file->SeekPosition += nBytes;

	if (file->file->entry.size < file->SeekPosition)
	{
		file->file->entry.size = (uint32_t)file->SeekPosition;
	}

	return 0;
}

int FAT32::ResizeFile(FAT32_OpenFile* file, uint64_t new_size)
{
	driver->ResizeFile(file->file->entry, new_size);
	file->file->entry.size = new_size;
	file->SeekPosition = 0;

	return 0;
}

void FAT32::AddToRecords(const std::string& path, DirEntry entry)
{
	FAT32_FolderStructure* parent = root;

	if (path != "~")
	{
		uint32_t last = 2;
		for (uint32_t i = 2; i < path.length(); i++)
		{
			if (path[i] == '/' || (i == (path.length() - 1)))
			{
				size_t size = i - last;
				if (i == (path.length() - 1))
				{
					size++;
				}

				std::string subdir = path.substr(last, size);
				last = i + 1;

				for (auto child : parent->children)
				{
					if (subdir == child->entry.name)
					{
						parent = child;
						break;
					}
				}
			}
		}
	}

	FAT32_FolderStructure* curr = new FAT32_FolderStructure();

	curr->parent = parent;
	curr->entry = entry;

	parent->children.push_back(curr);
}

void FAT32::DeleteFromRecords(const std::string& name)
{
	FAT32_FolderStructure* curr = root;

	uint32_t last = 2;
	for (uint32_t i = 2; i < name.length(); i++)
	{
		if (name[i] == '/' || (i == (name.length() - 1)))
		{
			size_t size = i - last;
			if (i == (name.length() - 1))
			{
				size++;
			}

			std::string subdir = name.substr(last, size);
			last = i + 1;

			for (size_t i = 0; i < curr->children.size(); i++)
			{
				auto child = curr->children[i];
				if (subdir == child->entry.name)
				{
					curr = child;
					break;
				}
			}
		}
	}

	auto idx = std::find(curr->parent->children.begin(), curr->parent->children.end(), curr);
	curr->parent->children.erase(idx);
	delete curr;
}

std::string FAT32::GetCurrentDirectory() const
{
	std::string name = "~";

	std::vector<FAT32_FolderStructure*> structs;
	FAT32_FolderStructure* curr = current;
	while (curr->parent)
	{
		structs.push_back(curr);
		curr = curr->parent;
	}

	for (auto it = structs.rbegin(); it != structs.rend(); ++it)
	{
		name += '/';
		name += (*it)->entry.name;
	}
	
	return name;
}

void FAT32::ListCurrent()
{
	for (auto elem : current->children)
	{
		//No need to display the parent and self directory
		if (elem->entry.name[0] == '.')
		{
			continue;
		}

		std::string flags;
		if (elem->entry.attributes & FILE_READ_ONLY) flags += '-'; else flags += 'w';
		if (elem->entry.attributes & FILE_HIDDEN) flags += 'h'; else flags += '-';
		if (elem->entry.attributes & FILE_SYSTEM) flags += 's'; else flags += '-';
		if (elem->entry.attributes & FILE_VOLUME_ID) flags += 'v'; else flags += '-';
		if (elem->entry.attributes & FILE_DIRECTORY) flags += 'd'; else flags += '-';
		if (elem->entry.attributes & FILE_ARCHIVE) flags += 'a'; else flags += '-';
		printf("%-30s - %s\n", elem->entry.name, flags.c_str());
	}
}

void FAT32::GoTo(char* name)
{
	size_t len = strlen(name);
	if ((memcmp(name, ".", 1) == 0) && (len == 1))
	{
		if (current->parent)
		{
			current = current->parent;
		}

		return;
	}
	if ((memcmp(name, "..", 2) == 0) && (len == 2))
	{
		return;
	}

	for (auto elem : current->children)
	{
		if (strcmp(elem->entry.name, name) == 0)
		{
			if ((elem->entry.attributes & FILE_DIRECTORY) & FILE_DIRECTORY)
			{
				current = elem;
			}

			break;
		}
	}
}

FAT32_FolderStructure* FAT32::RetrieveFromRoot(DirEntry entry)
{
	std::vector<FAT32_FolderStructure*> all = GetAllEntries(root);
	for (auto elem : all)
	{
		if (elem->entry.cluster == entry.cluster)
		{
			return elem;
		}
	}

	return nullptr;
}

FAT32_FolderStructure* FAT32::GetFolderByCluster(uint32_t cluster)
{
	if (root->entry.cluster == cluster)
	{
		return root;
	}

	std::vector<FAT32_FolderStructure*> all = GetAllEntries(root);
	for (auto elem : all)
	{
		if (elem->entry.cluster == cluster)
		{
			return elem;
		}
	}

	return nullptr;
}

std::vector<FAT32_FolderStructure*> FAT32::GetAllEntries(FAT32_FolderStructure* top)
{
	std::vector<FAT32_FolderStructure*> res = top->children;

	for (auto elem : top->children)
	{
		std::vector<FAT32_FolderStructure*> ret = GetAllEntries(elem);
		for (auto el : ret)
		{
			res.push_back(el);
		}
	}

	return res;
}
