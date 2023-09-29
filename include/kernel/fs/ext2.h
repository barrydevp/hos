#pragma once

#include <kernel/types.h>
#include <kernel/fs/vfs.h>
#include <kernel/spinlock.h>

// clang-format off
#define EXT2_SUPERBLOCK_MAGIC  0xEF53 ///< Magic value used to identify an ext2 filesystem.
#define EXT2_INDIRECT_BLOCKS   12     ///< Amount of indirect blocks in an inode.
#define EXT2_PATH_MAX          4096   ///< Maximum length of a pathname.
#define EXT2_MAX_SYMLINK_COUNT 8      ///< Maximum nesting of symlinks, used to prevent a loop.
#define EXT2_NAME_LEN          255    ///< The lenght of names inside directory entries.

// File types.
#define EXT2_S_IFMT   0xF000 ///< Format mask
#define EXT2_S_IFSOCK 0xC000 ///< Socket
#define EXT2_S_IFLNK  0xA000 ///< Symbolic link
#define EXT2_S_IFREG  0x8000 ///< Regular file
#define EXT2_S_IFBLK  0x6000 ///< Block device
#define EXT2_S_IFDIR  0x4000 ///< Directory
#define EXT2_S_IFCHR  0x2000 ///< Character device
#define EXT2_S_IFIFO  0x1000 ///< Fifo

// Permissions bit.
#define EXT2_S_ISUID 0x0800 ///< SUID
#define EXT2_S_ISGID 0x0400 ///< SGID
#define EXT2_S_ISVTX 0x0200 ///< Sticky Bit
#define EXT2_S_IRWXU 0x01C0 ///< rwx------- : User can read/write/execute
#define EXT2_S_IRUSR 0x0100 ///< -r-------- : User can read
#define EXT2_S_IWUSR 0x0080 ///< --w------- : User can write
#define EXT2_S_IXUSR 0x0040 ///< ---x------ : User can execute
#define EXT2_S_IRWXG 0x0038 ///< ----rwx--- : Group can read/write/execute
#define EXT2_S_IRGRP 0x0020 ///< ----r----- : Group can read
#define EXT2_S_IWGRP 0x0010 ///< -----w---- : Group can write
#define EXT2_S_IXGRP 0x0008 ///< ------x--- : Group can execute
#define EXT2_S_IRWXO 0x0007 ///< -------rwx : Others can read/write/execute
#define EXT2_S_IROTH 0x0004 ///< -------r-- : Others can read
#define EXT2_S_IWOTH 0x0002 ///< --------w- : Others can write
#define EXT2_S_IXOTH 0x0001 ///< ---------x : Others can execute
// clang-format on

// ============================================================================
// Data Structures
// ============================================================================

/// @brief Types of file in an EXT2 filesystem.
typedef enum ext2_file_type_t {
  ext2_file_type_unknown,          ///< Unknown type.
  ext2_file_type_regular_file,     ///< Regular file.
  ext2_file_type_directory,        ///< Directory.
  ext2_file_type_character_device, ///< Character device.
  ext2_file_type_block_device,     ///< Block device.
  ext2_file_type_named_pipe,       ///< Named pipe.
  ext2_file_type_socket,           ///< Socket
  ext2_file_type_symbolic_link     ///< Symbolic link.
} ext2_file_type_t;

/// @brief Status of a block.
typedef enum ext2_block_status_t {
  ext2_block_status_free     = 0, ///< The block is free.
  ext2_block_status_occupied = 1  ///< The block is occupied.
} ext2_block_status_t;

/// @brief The superblock contains all the information about the configuration
/// of the filesystem.
/// @details The primary copy of the superblock is stored at an offset of 1024
/// bytes from the start of the device, and it is essential to mounting the
/// filesystem. Since it is so important, backup copies of the superblock are
/// stored in block groups throughout the filesystem.
typedef struct ext2_superblock_t {
  /// @brief Total number of inodes in file system.
  uint32_t inodes_count;
  /// @brief Total number of blocks in file system
  uint32_t blocks_count;
  /// @brief Number of blocks reserved for superuser.
  uint32_t r_blocks_count;
  /// @brief Total number of unallocated blocks.
  uint32_t free_blocks_count;
  /// @brief Total number of unallocated inodes.
  uint32_t free_inodes_count;
  /// @brief Block number of the block containing the superblock.
  uint32_t first_data_block;
  /// @brief The number to shift 1024 to the left by to obtain the block size
  /// (log2 (block size) - 10).
  uint32_t log_block_size;
  /// @brief The number to shift 1024 to the left by to obtain the fragment
  /// size (log2 (fragment size) - 10).
  uint32_t log_frag_size;
  /// @brief Number of blocks in each block group.
  uint32_t blocks_per_group;
  /// @brief Number of fragments in each block group.
  uint32_t frags_per_group;
  /// @brief Number of inodes in each block group.
  uint32_t inodes_per_group;
  /// @brief Last mount time (in POSIX time).
  uint32_t mtime;
  /// @brief Last written time (in POSIX time).
  uint32_t wtime;
  /// @brief Number of times the volume has been mounted since its last
  /// consistency check (fsck).
  uint16_t mnt_count;
  /// @brief Number of mounts allowed before a consistency check (fsck) must
  /// be done.
  uint16_t max_mnt_count;
  /// @brief Ext2 signature (0xef53), used to help confirm the presence of
  /// Ext2 on a volume.
  uint16_t magic;
  /// @brief File system state.
  uint16_t state;
  /// @brief What to do when an error is detected.
  uint16_t errors;
  /// @brief Minor portion of version (combine with Major portion below to
  /// construct full version field).
  uint16_t minor_rev_level;
  /// @brief POSIX time of last consistency check (fsck).
  uint32_t lastcheck;
  /// @brief Interval (in POSIX time) between forced consistency checks
  /// (fsck).
  uint32_t checkinterval;
  /// @brief Operating system ID from which the filesystem on this volume was
  /// created.
  uint32_t creator_os;
  /// @brief Major portion of version (combine with Minor portion above to
  /// construct full version field).
  uint32_t rev_level;
  /// @brief User ID that can use reserved blocks.
  uint16_t def_resuid;
  /// @brief Group ID that can use reserved blocks.
  uint16_t def_resgid;

  // == Extended Superblock Fields ==========================================
  /// @brief First non-reserved inode in file system. (In versions < 1.0, this
  /// is fixed as 11)
  uint32_t first_ino;
  /// @brief Size of each inode structure in bytes. (In versions < 1.0, this
  /// is fixed as 128)
  uint16_t inode_size;
  /// @brief Block group that this superblock is part of (if backup copy).
  uint16_t block_group_nr;
  /// @brief Optional features present (features that are not required to read
  /// or write, but usually result in a performance increase).
  uint32_t feature_compat;
  /// @brief Required features present (features that are required to be
  /// supported to read or write)
  uint32_t feature_incompat;
  /// @brief Features that if not supported, the volume must be mounted
  /// read-only).
  uint32_t feature_ro_compat;
  /// @brief File system ID (what is output by blkid).
  uint8_t uuid[16];
  /// @brief Volume name (C-style string: characters terminated by a 0 byte).
  uint8_t volume_name[16];
  /// @brief Path volume was last mounted to (C-style string: characters
  /// terminated by a 0 byte).
  uint8_t last_mounted[64];
  /// @brief Compression algorithms used.
  uint32_t algo_bitmap;

  // == Performance Hints ===================================================
  /// @brief Number of blocks to preallocate for files.
  uint8_t prealloc_blocks;
  /// @brief Number of blocks to preallocate for directories.
  uint8_t prealloc_dir_blocks;
  /// @brief (Unused)
  uint16_t padding0;

  // == Journaling Support ==================================================
  /// @brief Journal ID
  uint8_t journal_uuid[16];
  /// @brief Inode number of journal file.
  uint32_t journal_inum;
  /// @brief Device number of journal file.
  uint32_t jounral_dev;
  /// @brief Start of list of inodes to delete.
  uint32_t last_orphan;

  // == Directory Indexing Support ==========================================
  /// @brief HTree hash seed.
  uint32_t hash_seed[4];
  /// @brief Ddefault hash version to use.
  uint8_t def_hash_version;
  /// @brief Padding.
  uint16_t padding1;
  /// @brief Padding.
  uint8_t padding2;

  // == Other Options =======================================================
  /// @brief The default mount options for the file system.
  uint32_t default_mount_options;
  /// @brief The ID of the first meta block group.
  uint32_t first_meta_block_group_id;
  /// @brief Reserved.
  uint8_t reserved[760];
} ext2_superblock_t;

/// @brief Entry of the Block Group Descriptor Table (BGDT).
typedef struct ext2_group_descriptor_t {
  /// @brief The block number of the block bitmap for this Block Group
  uint32_t block_bitmap;
  /// @brief The block number of the inode allocation bitmap for this Block Group.
  uint32_t inode_bitmap;
  /// @brief The block number of the starting block for the inode table for this Block Group.
  uint32_t inode_table;
  /// @brief Number of free blocks.
  uint16_t free_blocks_count;
  /// @brief Number of free inodes.
  uint16_t free_inodes_count;
  /// @brief Number of used directories.
  uint16_t used_dirs_count;
  /// @brief Padding.
  uint16_t pad;
  /// @brief Reserved.
  uint32_t reserved[3];
} ext2_group_descriptor_t;

/// @brief The ext2 inode.
typedef struct ext2_inode_t {
  /// @brief File mode
  uint16_t mode;
  /// @brief The user identifiers of the owners.
  uint16_t uid;
  /// @brief The size of the file in bytes.
  uint32_t size;
  /// @brief The time that the inode was accessed.
  uint32_t atime;
  /// @brief The time that the inode was created.
  uint32_t ctime;
  /// @brief The time that the inode was modified the last time.
  uint32_t mtime;
  /// @brief The time that the inode was deleted.
  uint32_t dtime;
  /// @brief The group identifiers of the owners.
  uint16_t gid;
  /// @brief Number of hard links.
  uint16_t links_count;
  /// @brief Blocks count.
  uint32_t blocks_count;
  /// @brief File flags.
  uint32_t flags;
  /// @brief OS dependant value.
  uint32_t osd1;
  /// @brief Mutable data.
  union {
    /// [60 byte] Blocks indices.
    struct {
      /// [48 byte]
      uint32_t dir_blocks[EXT2_INDIRECT_BLOCKS];
      /// [ 4 byte]
      uint32_t indir_block;
      /// [ 4 byte]
      uint32_t doubly_indir_block;
      /// [ 4 byte]
      uint32_t trebly_indir_block;
    } blocks;
    /// [60 byte]
    char symlink[60];
  } data;
  /// @brief Value used to indicate the file version (used by NFS).
  uint32_t generation;
  /// @brief Value indicating the block number containing the extended attributes.
  uint32_t file_acl;
  /// @brief For regular files this 32bit value contains the high 32 bits of the 64bit file size.
  uint32_t dir_acl;
  /// @brief Value indicating the location of the file fragment.
  uint32_t fragment_addr;
  /// @brief OS dependant structure.
  uint32_t osd2[3];
} ext2_inode_t;

/// @brief The header of an ext2 directory entry.
typedef struct ext2_dirent_t {
  /// Number of the inode that this directory entry points to.
  uint32_t inode;
  /// Length of this directory entry. Must be a multiple of 4.
  uint16_t rec_len;
  /// Length of the file name.
  uint8_t name_len;
  /// File type code.
  uint8_t file_type;
  /// File name.
  char name[EXT2_NAME_LEN];
} ext2_dirent_t;

/// @brief The details regarding the filesystem.
typedef struct ext2_filesystem_t {
  /// Pointer to the block device.
  vfs_file_t *block_device;
  /// Device superblock, contains important information.
  ext2_superblock_t superblock;
  /// Block Group Descriptor / Block groups.
  ext2_group_descriptor_t *block_groups;
  /// EXT2 memory cache for buffers.
  /// TODO: This is for slab allocator
  // kmem_cache_t *ext2_buffer_cache;
  /// Root FS node (attached to mountpoint).
  vfs_file_t *root;
  /// List of opened files.
  list_head opened_files;

  /// Size of one block.
  uint32_t block_size;

  /// Number of inodes that fit in a block.
  uint32_t inodes_per_block_count;
  /// Number of blocks that fit in a block.
  uint32_t blocks_per_block_count;
  /// Number of blocks groups.
  uint32_t block_groups_count;
  /// Number of block pointers per block.
  uint32_t pointers_per_block;
  /// Index in terms of blocks where the BGDT starts.
  uint32_t bgdt_start_block;
  /// Index in terms of blocks where the BGDT ends.
  uint32_t bgdt_end_block;
  /// The number of blocks containing the BGDT
  uint32_t bgdt_length;

  /// Index of indirect blocks.
  uint32_t indirect_blocks_index;
  /// Index of doubly-indirect blocks.
  uint32_t doubly_indirect_blocks_index;
  /// Index of trebly-indirect blocks.
  uint32_t trebly_indirect_blocks_index;

  /// Spinlock for protecting filesystem operations.
  spinlock_t spinlock;
} ext2_filesystem_t;

/// @brief Structure used when searching for a directory entry.
typedef struct ext2_direntry_search_t {
  /// Pointer to the direntry where we store the search results.
  ext2_dirent_t *direntry;
  /// The inode of the parent directory.
  ino_t parent_inode;
  /// The index of the block where the direntry resides.
  uint32_t block_index;
  /// The offest of the direntry inside the block.
  uint32_t block_offset;
} ext2_direntry_search_t;

// ============================================================================
// Forward Declaration of Functions
// ============================================================================

static ext2_block_status_t ext2_check_bitmap_bit(uint8_t *buffer,
                                                 uint32_t index);
static void ext2_set_bitmap_bit(uint8_t *buffer, uint32_t index,
                                ext2_block_status_t status);

static int ext2_read_superblock(ext2_filesystem_t *fs);
static int ext2_write_superblock(ext2_filesystem_t *fs);
static int ext2_read_block(ext2_filesystem_t *fs, uint32_t block_index,
                           uint8_t *buffer);
static int ext2_write_block(ext2_filesystem_t *fs, uint32_t block_index,
                            uint8_t *buffer);
static int ext2_read_bgdt(ext2_filesystem_t *fs);
static int ext2_write_bgdt(ext2_filesystem_t *fs);
static int ext2_read_inode(ext2_filesystem_t *fs, ext2_inode_t *inode,
                           uint32_t inode_index);
static int ext2_write_inode(ext2_filesystem_t *fs, ext2_inode_t *inode,
                            uint32_t inode_index);

static vfs_file_t *ext2_open(const char *path, int flags, mode_t mode);
static int ext2_unlink(const char *path);
static int ext2_close(vfs_file_t *file);
static ssize_t ext2_read(vfs_file_t *file, char *buffer, off_t offset,
                         size_t nbyte);
static ssize_t ext2_write(vfs_file_t *file, const void *buffer, off_t offset,
                          size_t nbyte);
static off_t ext2_lseek(vfs_file_t *file, off_t offset, int whence);
static int ext2_fstat(vfs_file_t *file, stat_t *stat);
static int ext2_ioctl(vfs_file_t *file, int request, void *data);
static int ext2_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff,
                         size_t count);

static int ext2_mkdir(const char *path, mode_t mode);
static int ext2_rmdir(const char *path);
static int ext2_stat(const char *path, stat_t *stat);
static vfs_file_t *ext2_creat(const char *path, mode_t permission);
static vfs_file_t *ext2_mount(vfs_file_t *block_device, const char *path);

/// @brief Initializes the EXT2 drivers.
/// @return 0 on success, 1 on error.
int ext2_init();

/// @brief De-initializes the EXT2 drivers.
/// @return 0 on success, 1 on error.
int ext2_finalize();
