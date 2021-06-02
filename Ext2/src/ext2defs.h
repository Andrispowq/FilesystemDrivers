#ifndef EXT2_DEFS_H
#define EXT2_DEFS_H

#include <iostream>
#include <fstream>

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

#define EXT2_SIGNATURE 0xEF53

PACK(struct superblock
{
	uint32_t total_inodes;
	uint32_t total_blocks;
	uint32_t superuser_blocks;
	uint32_t unallocated_blocks;
	uint32_t unallocated_inodes;
	uint32_t superblock;
	uint32_t block_size;
	uint32_t fragment_size;
	uint32_t blocks_per_group;
	uint32_t fragments_per_group;
	uint32_t last_mount_time;
	uint32_t last_written_time;
	uint16_t number_of_mounts_since_check;
	uint16_t number_of_mounts_before_check;
	uint16_t signature;
	uint16_t fs_state;
	uint16_t error_protocol;
	uint16_t version_minor;
	uint32_t last_check;
	uint32_t interval_of_forced_checks;
	uint32_t creator_OS_ID;
	uint32_t version_major;
	uint16_t UID_reserved_blocks;
	uint16_t GID_reserved_blocks;

	uint32_t first_usuable_inode;
	uint16_t inode_size;
	uint16_t block_group;
	uint32_t optional_features;
	uint32_t required_features;
	uint32_t features_needed_else_read_only;
	uint8_t fs_ID[16];
	uint8_t volume_name[16];
	uint32_t mount_path[16];
	uint32_t compression_algorithm;
	uint8_t number_of_blocks_to_allocate_file;
	uint8_t number_of_blocks_to_allocate_directory;
	uint16_t unused;
	uint8_t journal_ID[16];
	uint32_t journal_inode;
	uint32_t journal_device;
	uint32_t head_of_orphan_inodes;
	uint8_t unused1[787];
});

enum class FilesystemState
{
	clean = 0x1,
	error = 0x2
};

enum class ErrorProtocol
{
	ignore = 0x1,
	remount_as_read_only = 0x2,
	kernel_panic = 0x3
};

enum class OS_ID
{
	Linux = 0x0,
	GNU_HURD = 0x1,
	MASIX = 0x2,
	FreeBSD = 0x3,
	Other = 0x4
};

#endif