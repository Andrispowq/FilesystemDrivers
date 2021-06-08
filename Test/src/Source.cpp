#include "FAT32.h"
#include "ext2driver.h"

#include <fstream>
#include <string>

int main()
{
	ext2driver ext2("../Ext2/res/ext2.img");

	FAT32 fat32("../FAT32/res/HackOS.img");
	FAT32_FolderStructure* root = fat32.GetRoot();
	FAT32Driver* driver = fat32.GetDriver();

	FAT32_OpenFile* file = nullptr;
	while (true)
	{
		std::cout << "Desktop@FATImage" << fat32.GetCurrentDirectory() << "$ ";

		std::string in; 
		std::getline(std::cin, in);

		if (in == "ls")
		{
			fat32.ListCurrent();
		}
		else if (in == "quit")
		{
			break;
		}
		else if (in.substr(0, 6) == "mkdir ")
		{
			fat32.CreateFile(fat32.GetCurrentDirectory(), in.substr(6, in.length() - 6), FILE_DIRECTORY, 0);
		}
		else if (in.substr(0, 6) == "touch ")
		{
			fat32.CreateFile(fat32.GetCurrentDirectory(), in.substr(6, in.length() - 6), FILE_ARCHIVE, 2048);
		}
		else if (in.substr(0, 3) == "cd ")
		{
			fat32.GoTo((char*)in.substr(3, in.length() - 3).c_str());
		}
		else if (in.substr(0, 5) == "open ")
		{
			if (file)
			{
				fat32.CloseFile(file);
			}

			file = fat32.OpenFile(fat32.GetCurrentDirectory() + '/' + in.substr(5, in.length() - 5));
		}
		else if (in.substr(0, 5) == "close")
		{
			if (!file)
			{
				continue;
			}

			fat32.CloseFile(file);
			file = nullptr;
		}
		else if (in.substr(0, 5) == "read ")
		{
			if (!file)
			{
				continue;
			}

			uint32_t size = std::atoi(in.substr(5, in.length() - 5).c_str());

			uint8_t* buffer = new uint8_t[size];
			fat32.ReadFile(file, buffer, size);
			printf("Bytes read: %s\n", buffer);
			delete[] buffer;
		}
	}

	return 0;
}