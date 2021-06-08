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

PACK(struct ext2_superblock
{
	uint32_t total_inodes;
	uint32_t total_blocks;
	uint32_t superuser_blocks;
	uint32_t unallocated_blocks;
	uint32_t unallocated_inodes;
	uint32_t superblock_block;
	uint32_t block_size;
	uint32_t fragment_size;
	uint32_t blocks_per_group;
	uint32_t fragments_per_group;
	uint32_t inodes_per_group;
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
	uint8_t mount_path[64];
	uint32_t compression_algorithm;
	uint8_t number_of_blocks_to_allocate_file;
	uint8_t number_of_blocks_to_allocate_directory;
	uint16_t unused;
	uint8_t journal_ID[16];
	uint32_t journal_inode;
	uint32_t journal_device;
	uint32_t head_of_orphan_inodes;
	uint8_t unused1[788];
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

PACK(struct ext2_bgd
{
	uint32_t block_bitmap;
	uint32_t inode_bitmap;
	uint32_t inode_table;
	uint16_t unallocated_blocks;
	uint16_t unallocated_inodes;
	uint16_t directory_count;
	uint8_t unused[14];
});

PACK(struct ext2_inode
{
	uint16_t type_permissions;
	uint16_t UID;
	uint32_t size_low;
	uint32_t atime;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t dtime;
	uint16_t GID;
	uint16_t hard_links;
	uint32_t sectors_occupied;
	uint32_t flags;
	uint32_t OS_specific0;
	uint32_t direct[12];
	uint32_t single_indirect;
	uint32_t double_indirect;
	uint32_t triple_indirect;
	uint32_t generation_number;
	uint32_t extended_attributes;
	uint32_t size_high;
	uint32_t fragment;
	uint8_t OS_specific2[12];
});

#define EXT2_PERMISSION_EXECUTE 0x1
#define EXT2_PERMISSION_WRITE 0x2
#define EXT2_PERMISSION_READ 0x4
#define EXT2_PERMISSION_GROUP_EXECUTE 0x8
#define EXT2_PERMISSION_GROUP_WRITE 0x10
#define EXT2_PERMISSION_GROUP_READ 0x20
#define EXT2_PERMISSION_USER_EXECUTE 0x40
#define EXT2_PERMISSION_USER_WRITE 0x80
#define EXT2_PERMISSION_USER_READ 0x100
#define EXT2_PERMISSION_STICKY_BIT 0x200
#define EXT2_PERMISSION_SET_GID 0x400
#define EXT2_PERMISSION_SET_UID 0x800

#define EXT2_TYPE_FIFO 0x1000
#define EXT2_TYPE_CHAR_DEV 0x2000
#define EXT2_TYPE_DIR 0x4000
#define EXT2_TYPE_BLOCK_DEV 0x8000
#define EXT2_TYPE_SYM_LINK 0xA000
#define EXT2_TYPE_UNIX_SOCKET 0xC000

#define EXT2_INODE_FLAG_SECURE_DELETION 0x1
#define EXT2_INODE_FLAG_KEEP_COPY_WHEN_DELETED 0x2
#define EXT2_INODE_FLAG_FILE_COMPRESSIN 0x4
#define EXT2_INODE_FLAG_IMMEDIATE_WRITE 0x8
#define EXT2_INODE_FLAG_IMMUTABLE_FILE 0x10
#define EXT2_INODE_FLAG_APPEND_ONLY 0x20
#define EXT2_INODE_FLAG_NOT_INCLUDED_IN_DUMP 0x40
#define EXT2_INODE_FLAG_ACCES_TIME_CONSTANT 0x80
#define EXT2_INODE_FLAG_HASH_INDEXED_DIRECTORY 0x10000
#define EXT2_INODE_FLAG_AFS_DIRECTORY 0x20000
#define EXT2_INODE_FLAG_JOURNAL_FILE_DATA 0x40000

PACK(struct directory_entry
{
	uint32_t inode;
	uint16_t size;
	uint8_t name_length_low;
	uint8_t type_indicator;
	uint8_t name[];
});

struct dir_entry
{
	uint32_t inode;
	uint32_t size;
	uint32_t type_indicator;
	uint8_t name[128];
};

enum class type_indicator
{
	unkown_type,
	regular_file,
	directory,
	char_device,
	block_device,
	FIFO,
	socket,
	symlink
};

#endif