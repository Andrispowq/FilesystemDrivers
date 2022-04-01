                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         #include "FAT32.h"
#include "ext2driver.h"
#include "exFATdriver.h"

#include <fstream>
#include <string>

int main()
{
	ext2::ext2driver ext2("../Ext2/res/ext2.img");
	ext2::DirEntry dirEntry2;
	ext2.OpenFile("~/kernel.elf", &dirEntry2);

	uint64_t unix_time = 1648575577;
	float second = unix_time % 60;
	float minute = (unix_time /= 60) % 60;
	float hour = (unix_time /= 60) % 24;
	float day = (unix_time /= 24) % 366;
	float year = (unix_time /= 365) + 1970;

	char* buf2 = new char[dirEntry2.size];
	ext2.ReadFile(dirEntry2, 0, buf2, dirEntry2.size);
	delete[] buf2;

	exFAT::exFATDriver exFAT("../exFAT/res/exFAT.img");

	std::vector<exFAT::DirEntry> entries = exFAT.GetDirectories(6, 0, false);

	exFAT::DirEntry entry;
	exFAT.OpenFile("~/kernel.elf", &entry);

	char* buf = new char[entry.size];
	exFAT.ReadFile(entry, 0, buf, entry.size);
	delete[] buf;

	FAT32 fat32("../FAT32/res/HackOS.img");
	FAT32_FolderStructure* root = fat32.GetRoot();
	FAT32Driver* driver = fat32.GetDriver();

	/*FAT32_Data data;
	data.name = "test";
	data.TotalSectors = 100000;
	FAT32Driver* new_file = FAT32Driver::CreateFAT32(data);
	delete new_file;*/

	FAT32_OpenFile* file_to_delete = fat32.OpenFile("~/USR/FILES/playlist_file.json");
	if(file_to_delete) fat32.DeleteFile(file_to_delete);

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
			char str[] = R"("{
  "playlist1": [
    {
      "source": "album:3N7eWDCvfWv34xWNohdHjO",
      "track": 0,
      "uri": "4hQ6UGyWQIGJmHSo0J88JW",
      "name": "Back to you"
    },
    {
      "source": "playlist:37i9dQZF1E8NIqb7ifZ0x5",
      "track": 0,
      "uri": "5yP7ZMMAbaZQMfBirBBCen",
      "name": "Rajosan"
    },
    {
      "source": "album:7p1fX8aUySrBdx4WSYspOu",
      "track": 2,
      "uri": "4nVBt6MZDDP6tRVdQTgxJg",
      "name": "Story of my life"
    },
    {
      "source": "playlist:37i9dQZF1EQncLwOalG3K7",
      "track": 9,
      "uri": "41Fflg7qHiVOD6dEPvsCzO",
      "name": "Worth it"
    },
    {
      "source": "playlist:37i9dQZF1E8BAmzD4F9EWe",
      "track": 0,
      "uri": "0Cy7wt6IlRfBPHXXjmZbcP",
      "name": "Love me like you do"
    }
  ]
}")";

			FAT32_OpenFile* file = fat32.CreateFile(fat32.GetCurrentDirectory(), in.substr(6, in.length() - 6), FILE_ARCHIVE, 2048);
			fat32.WriteFile(file, str, sizeof(str));
			fat32.CloseFile(file);
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