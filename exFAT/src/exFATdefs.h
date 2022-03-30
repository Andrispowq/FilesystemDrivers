#ifndef EX_FAT_DEFS_H
#define EX_FAT_DEFS_H

#include <iostream>
#include <fstream>

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

#define END_CLUSTER 0xFFFFFFF8
#define BAD_CLUSTER 0xFFFFFFF7
#define FREE_CLUSTER 0x00000000

#define FILE_READ_ONLY 0x01
#define FILE_HIDDEN 0x02
#define FILE_SYSTEM 0x04
#define FILE_VOLUME_ID 0x08
#define FILE_DIRECTORY 0x10
#define FILE_ARCHIVE 0x20

#define ENTRY_END 0x00
#define ENTRY_FILE 0x85
#define ENTRY_STREAM 0xC0
#define ENTRY_FILENAME 0xC1

namespace exFAT
{

	struct exFAT_BootSector
	{
		uint8_t JumpInstruction[3];
		uint8_t OEM[8];
		uint8_t Zero[53];

		uint64_t PartitionOffset;
		uint64_t VolumeLength;

		uint32_t FATOffset;
		uint32_t FATLength;
		uint32_t ClusterHeapOffset;
		uint32_t ClusterCount;
		uint32_t RootDirectoryCluster;
		uint32_t SerialNumber;

		uint16_t Revision;
		uint16_t Flags;

		uint8_t SectorShift;
		uint8_t ClusterShift;
		uint8_t NumberOfFATs;
		uint8_t DriveSelect;
		uint8_t UsagePercentage;
		uint8_t Reserved[7];

		uint8_t BootCode[390];
		uint8_t BootablePartitionSignature[2];
	};

	struct FileEntryGeneral
	{
		uint8_t EntryType;
		uint8_t Data[31];
	};

	struct FileEntry
	{
		uint8_t EntryType = ENTRY_FILE;
		uint8_t SecondaryEntries;

		uint16_t Checksum;
		uint16_t FileAttributes;
		uint16_t Reserved0;

		uint32_t CreationTime;
		uint32_t ModificationTime;
		uint32_t AccessTime;

		uint8_t CreationMilliseconds;
		uint8_t ModificationMilliseconds;
		uint8_t CreationUTC;
		uint8_t ModificationUTC;
		uint8_t AccessUTC;
		uint8_t Reserved1[7];
	};

	struct StreamEntry
	{
		uint8_t EntryType = ENTRY_STREAM;
		uint8_t SecondaryFlags;
		uint8_t Reserved0;
		uint8_t NameLength;

		uint16_t NameHash;
		uint16_t Reserved1;

		uint64_t ValidDataLength;
		uint32_t Reserved2;
		uint32_t FirstCluster;

		uint64_t DataLength;
	};

	struct FileNameEntry
	{
		uint8_t EntryType = ENTRY_FILENAME;
		uint8_t Flags;

		uint16_t FileName[15];
	};

	struct DirEntry
	{
		char name[128] = { 0 };
		uint32_t cluster = 0;
		uint32_t size = 0;
		uint8_t attributes = 0;

		uint32_t parentCluster = 0;
		uint32_t offsetInParentCluster = 0;
	};

};

#endif