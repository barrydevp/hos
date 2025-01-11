/// Copyright (c) 2014-2024 MentOs-Team
/// Copyright (c) 2022-2024 Minh Hai Dao (barrydevp)

#include <kernel/fs/ext2.h>

#include <kernel/kernel.h>
#include <kernel/process/scheduler.h>
#include <kernel/process/task.h>
#include <kernel/memory/mmu.h>
#include <kernel/spinlock.h>
#include <kernel/fs/vfs_types.h>
#include <kernel/errno.h>
#include <kernel/fs/vfs.h>
#include <kernel/assert.h>
#include <kernel/libgen.h>
#include <kernel/string.h>
#include <kernel/bitops.h>
#include <kernel/stdio.h>
#include <kernel/fcntl.h>

#include <kernel/printf.h>

// ============================================================================
// Virtual FileSystem (VFS) Operaions
// ============================================================================

/// Filesystem general operations.
static struct vfs_sys_operations_t ext2_sys_operations = {
  .mkdir_f = ext2_mkdir,
  .rmdir_f = ext2_rmdir,
  .stat_f  = ext2_stat,
  .creat_f = ext2_creat
};

/// Filesystem file operations.
static struct vfs_file_operations_t ext2_fs_operations = {
  .open_f     = ext2_open,
  .unlink_f   = ext2_unlink,
  .close_f    = ext2_close,
  .read_f     = ext2_read,
  .write_f    = ext2_write,
  .lseek_f    = ext2_lseek,
  .stat_f     = ext2_fstat,
  .ioctl_f    = ext2_ioctl,
  .getdents_f = ext2_getdents
};

// ============================================================================
// Debugging Support Functions
// ============================================================================

/// @brief Turns an UUID to string.
/// @param uuid the UUID to turn to string.
/// @return the string representing the UUID.
static const char *uuid_to_string(uint8_t uuid[16]) {
  static char s[33] = { 0 };
  sprintf(
    s, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
    uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14],
    uuid[15]);
  return s;
}

/// @brief Turns an ext2_file_type to string.
/// @param ext2_type the ext2_file_type to turn to string.
/// @return the string representing the ext2_file_type.
static const char *ext2_file_type_to_string(ext2_file_type_t ext2_type) {
  if (ext2_type == ext2_file_type_regular_file)
    return "REG";
  if (ext2_type == ext2_file_type_directory)
    return "DIR";
  if (ext2_type == ext2_file_type_character_device)
    return "CHR";
  if (ext2_type == ext2_file_type_block_device)
    return "BLK";
  if (ext2_type == ext2_file_type_named_pipe)
    return "FIFO";
  if (ext2_type == ext2_file_type_socket)
    return "SOCK";
  if (ext2_type == ext2_file_type_symbolic_link)
    return "LNK";
  return "UNK";
}

static int ext2_file_type_to_vfs_file_type(int ext2_type) {
  if (ext2_type == ext2_file_type_regular_file)
    return DT_REG;
  if (ext2_type == ext2_file_type_directory)
    return DT_DIR;
  if (ext2_type == ext2_file_type_character_device)
    return DT_CHR;
  if (ext2_type == ext2_file_type_block_device)
    return DT_BLK;
  if (ext2_type == ext2_file_type_named_pipe)
    return DT_FIFO;
  if (ext2_type == ext2_file_type_socket)
    return DT_SOCK;
  if (ext2_type == ext2_file_type_symbolic_link)
    return DT_LNK;
  return DT_UNKNOWN;
}

/// @brief Turns the time to string.
/// @param time the UNIX time to turn to string.
/// @return time turned to string.
static const char *time_to_string(uint32_t time) {
  static char s[250] = { 0 };
  tm_t *tm           = localtime(&time);
  sprintf(s, "%2d/%2d %2d:%2d", tm->tm_mon, tm->tm_mday, tm->tm_hour,
          tm->tm_min);
  return s;
}

/// @brief Checks if the requests in flags are valid.
/// @param flags the flags to check.
/// @param mask the mask to check against.
/// @param uid the uid of the owner.
/// @param gid the gid of the owner.
/// @return true on success, false otherwise.
static bool_t ext2_valid_permissions(int flags, mode_t mask, uid_t uid,
                                     gid_t gid) {
  // Check the permissions.
  task_struct *task = scheduler_get_current_process();
  // The current task is the owner.
  if (task->uid == uid) {
    if (!bitmask_check(mask, S_IRUSR))
      return false;
    if (bitmask_check(flags, O_WRONLY) && !bitmask_check(mask, S_IWUSR))
      return false;
    if (bitmask_check(flags, O_RDWR) &&
        (!bitmask_check(mask, S_IRUSR) || !bitmask_check(mask, S_IWUSR)))
      return false;
  }
  if (task->gid == gid) {
    if (!bitmask_check(mask, S_IRGRP))
      return false;
    if (bitmask_check(flags, O_WRONLY) && !bitmask_check(mask, S_IWGRP))
      return false;
    if (bitmask_check(flags, O_RDWR) &&
        (!bitmask_check(mask, S_IRGRP) || !bitmask_check(mask, S_IWGRP)))
      return false;
  }
  if ((task->uid != uid) && (task->gid != gid)) {
    if (!bitmask_check(mask, S_IROTH))
      return false;
    if (bitmask_check(flags, O_WRONLY) && !bitmask_check(mask, S_IWOTH))
      return false;
    if (bitmask_check(flags, O_RDWR) &&
        (!bitmask_check(mask, S_IROTH) || !bitmask_check(mask, S_IWOTH)))
      return false;
  }
  return true;
}

/// @brief Dumps on debugging output the superblock.
/// @param sb the object to dump.
static void ext2_dump_superblock(ext2_superblock_t *sb) {
  dprintf("inodes_count          : %u\n", sb->inodes_count);
  dprintf("blocks_count          : %u\n", sb->blocks_count);
  dprintf("r_blocks_count        : %u\n", sb->r_blocks_count);
  dprintf("free_blocks_count     : %u\n", sb->free_blocks_count);
  dprintf("free_inodes_count     : %u\n", sb->free_inodes_count);
  dprintf("first_data_block      : %u\n", sb->first_data_block);
  dprintf("log_block_size        : %u\n", sb->log_block_size);
  dprintf("log_frag_size         : %u\n", sb->log_frag_size);
  dprintf("blocks_per_group      : %u\n", sb->blocks_per_group);
  dprintf("frags_per_group       : %u\n", sb->frags_per_group);
  dprintf("inodes_per_group      : %u\n", sb->inodes_per_group);
  dprintf("mtime                 : %s\n", time_to_string(sb->mtime));
  dprintf("wtime                 : %s\n", time_to_string(sb->wtime));
  dprintf("mnt_count             : %d\n", sb->mnt_count);
  dprintf("max_mnt_count         : %d\n", sb->max_mnt_count);
  dprintf("magic                 : 0x%0x (== 0x%0x)\n", sb->magic,
          EXT2_SUPERBLOCK_MAGIC);
  dprintf("state                 : %d\n", sb->state);
  dprintf("errors                : %d\n", sb->errors);
  dprintf("minor_rev_level       : %d\n", sb->minor_rev_level);
  dprintf("lastcheck             : %s\n", time_to_string(sb->lastcheck));
  dprintf("checkinterval         : %u\n", sb->checkinterval);
  dprintf("creator_os            : %u\n", sb->creator_os);
  dprintf("rev_level             : %u\n", sb->rev_level);
  dprintf("def_resuid            : %u\n", sb->def_resuid);
  dprintf("def_resgid            : %u\n", sb->def_resgid);
  dprintf("first_ino             : %u\n", sb->first_ino);
  dprintf("inode_size            : %u\n", sb->inode_size);
  dprintf("block_group_nr        : %u\n", sb->block_group_nr);
  dprintf("feature_compat        : %u\n", sb->feature_compat);
  dprintf("feature_incompat      : %u\n", sb->feature_incompat);
  dprintf("feature_ro_compat     : %u\n", sb->feature_ro_compat);
  dprintf("uuid                  : %s\n", uuid_to_string(sb->uuid));
  dprintf("volume_name           : %s\n", (char *)sb->volume_name);
  dprintf("last_mounted          : %s\n", (char *)sb->last_mounted);
  dprintf("algo_bitmap           : %u\n", sb->algo_bitmap);
  dprintf("prealloc_blocks       : %u\n", sb->prealloc_blocks);
  dprintf("prealloc_dir_blocks   : %u\n", sb->prealloc_dir_blocks);
  dprintf("journal_uuid          : %s\n", uuid_to_string(sb->journal_uuid));
  dprintf("journal_inum          : %u\n", sb->journal_inum);
  dprintf("jounral_dev           : %u\n", sb->jounral_dev);
  dprintf("last_orphan           : %u\n", sb->last_orphan);
  dprintf("hash_seed             : %u %u %u %u\n", sb->hash_seed[0],
          sb->hash_seed[1], sb->hash_seed[2], sb->hash_seed[3]);
  dprintf("def_hash_version      : %u\n", sb->def_hash_version);
  dprintf("default_mount_options : %u\n", sb->default_mount_options);
  dprintf("first_meta_bg         : %u\n", sb->first_meta_block_group_id);
}

/// @brief Dumps on debugging output the group descriptor.
/// @param gd the object to dump.
static void ext2_dump_group_descriptor(ext2_group_descriptor_t *gd) {
  dprintf("block_bitmap          : %u\n", gd->block_bitmap);
  dprintf("inode_bitmap          : %u\n", gd->inode_bitmap);
  dprintf("inode_table           : %u\n", gd->inode_table);
  dprintf("free_blocks_count     : %u\n", gd->free_blocks_count);
  dprintf("free_inodes_count     : %u\n", gd->free_inodes_count);
  dprintf("used_dirs_count       : %u\n", gd->used_dirs_count);
}

/// @brief Dumps on debugging output the inode.
/// @param inode the object to dump.
static void ext2_dump_inode(ext2_inode_t *inode) {
  char mask[32];
  tm_t *timeinfo;
  strmode(inode->mode, mask);
  dprintf("   Size : %6u N. Blocks : %4u\n", inode->size, inode->blocks_count);
  dprintf(" Access : (%4d/%s) Uid: (%d, %s) Gid: (%d, %s)\n", inode->mode, mask,
          inode->uid, "None", inode->gid, "None");
  timeinfo = localtime(&inode->atime);
  dprintf(" Access : %4d-%02d-%02d %2d:%2d:%2d (%d)\n", timeinfo->tm_year,
          timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour,
          timeinfo->tm_min, timeinfo->tm_sec, inode->atime);
  timeinfo = localtime(&inode->mtime);
  dprintf(" Modify : %4d-%02d-%02d %2d:%2d:%2d (%d)\n", timeinfo->tm_year,
          timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour,
          timeinfo->tm_min, timeinfo->tm_sec, inode->mtime);
  timeinfo = localtime(&inode->ctime);
  dprintf(" Change : %4d-%02d-%02d %2d:%2d:%2d (%d)\n", timeinfo->tm_year,
          timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour,
          timeinfo->tm_min, timeinfo->tm_sec, inode->ctime);
  timeinfo = localtime(&inode->dtime);
  dprintf(" Delete : %4d-%02d-%02d %2d:%2d:%2d (%d)\n", timeinfo->tm_year,
          timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour,
          timeinfo->tm_min, timeinfo->tm_sec, inode->dtime);
  dprintf("  Links : %2u Flags : %d\n", inode->links_count, inode->flags);
  dprintf(" Blocks : [ ");
  for (int i = 0; i < EXT2_INDIRECT_BLOCKS; ++i) {
    if (inode->data.blocks.dir_blocks[i])
      dprintf("%u ", inode->data.blocks.dir_blocks[i]);
  }
  dprintf("]\n");
  dprintf("IBlocks : %u\n", inode->data.blocks.indir_block);
  dprintf("DBlocks : %u\n", inode->data.blocks.doubly_indir_block);
  dprintf("TBlocks : %u\n", inode->data.blocks.trebly_indir_block);
  dprintf("Symlink : %s\n", inode->data.symlink);
  dprintf("Generation : %u file_acl : %u dir_acl : %u\n", inode->generation,
          inode->file_acl, inode->dir_acl);
  (void)timeinfo;
}

/// @brief Dumps on debugging output the dirent.
/// @param dirent the object to dump.
static void ext2_dump_dirent(ext2_dirent_t *dirent) {
  dprintf("Inode: %4u Rec. Len.: %4u Name Len.: %4u Type:%4s Name: %s\n",
          dirent->inode, dirent->rec_len, dirent->name_len,
          ext2_file_type_to_string(dirent->file_type), dirent->name);
}

/// @brief Dumps on debugging output the BGDT.
/// @param fs the filesystem of which we print the BGDT.
static void ext2_dump_bgdt(ext2_filesystem_t *fs) {
  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);
  for (uint32_t i = 0; i < fs->block_groups_count; ++i) {
    // Get the pointer to the current group descriptor.
    ext2_group_descriptor_t *gd = &(fs->block_groups[i]);
    dprintf("Block Group Descriptor [%u] @ %u:\n", i,
            fs->bgdt_start_block + i * fs->superblock.blocks_per_group);
    dprintf("    block_bitmap : %u\n", gd->block_bitmap);
    dprintf("    inode_bitmap : %u\n", gd->inode_bitmap);
    dprintf("    inode_table  : %u\n", gd->inode_table);
    dprintf("    Used Dirs    : %u\n", gd->used_dirs_count);
    dprintf("    Free Blocks  : %4u of %u\n", gd->free_blocks_count,
            fs->superblock.blocks_per_group);
    dprintf("    Free Inodes  : %4u of %u\n", gd->free_inodes_count,
            fs->superblock.inodes_per_group);
    // Dump the block bitmap.
    ext2_read_block(fs, gd->block_bitmap, cache);
    dprintf("    Block Bitmap at %u\n", gd->block_bitmap);
    for (uint32_t j = 0; j < fs->block_size; ++j) {
      if ((j % 8) == 0)
        dprintf("        Block index: %4u, Bitmap: %s\n", j / 8,
                dec_to_binary(cache[j / 8], 8));
      if (!ext2_check_bitmap_bit(cache, j)) {
        dprintf(
          "    First free block in group is in block %u, the linear index is %u\n",
          j / 8, j);
        break;
      }
    }
    // Dump the block bitmap.
    ext2_read_block(fs, gd->inode_bitmap, cache);
    dprintf("    Inode Bitmap at %d\n", gd->inode_bitmap);
    for (uint32_t j = 0; j < fs->block_size; ++j) {
      if ((j % 8) == 0)
        dprintf("        Block index: %4d, Bitmap: %s\n", j / 8,
                dec_to_binary(cache[j / 8], 8));
      if (!ext2_check_bitmap_bit(cache, j)) {
        dprintf(
          "    First free block in group is in block %d, the linear index is %d\n",
          j / 8, j);
        break;
      }
    }
  }
  // kmem_cache_free(cache);
  kfree(cache);
}

/// @brief Dumps on debugging output the filesystem.
/// @param fs the object to dump.
static void ext2_dump_filesystem(ext2_filesystem_t *fs) {
  dprintf("block_device          : 0x%x\n", fs->block_device);
  dprintf("superblock            : 0x%x\n", fs->superblock);
  dprintf("block_groups          : 0x%x\n", fs->block_groups);
  dprintf("root                  : 0x%x\n", fs->root);
  dprintf("block_size            : %d\n", fs->block_size);
  dprintf("inodes_per_block_count: %d\n", fs->inodes_per_block_count);
  dprintf("blocks_per_block_count: %d\n", fs->blocks_per_block_count);
  dprintf("block_groups_count    : %d\n", fs->block_groups_count);
  dprintf("pointers_per_block    : %d\n", fs->pointers_per_block);
  dprintf("bgdt_start_block      : %d\n", fs->bgdt_start_block);
  dprintf("bgdt_end_block        : %d\n", fs->bgdt_end_block);
  dprintf("bgdt_length           : %d\n", fs->bgdt_length);
}

// ============================================================================
// EXT2 Core Functions
// ============================================================================

/// @brief Determining which block group contains an inode.
/// @param fs the ext2 filesystem structure.
/// @param inode_index the inode index.
/// @return the group index.
/// @details Remember that inode addressing starts from 1.
static uint32_t ext2_get_group_index_from_inode(ext2_filesystem_t *fs,
                                                uint32_t inode_index) {
  assert(inode_index != 0 && "Your are trying to access inode 0.");
  return (inode_index - 1) / fs->superblock.inodes_per_group;
}

/// @brief Determining the offest of the inode inside the block group.
/// @param fs the ext2 filesystem structure.
/// @param inode_index the inode index.
/// @return the offset of the inode inside the group.
/// @details Remember that inode addressing starts from 1.
static uint32_t ext2_get_inode_offest_in_group(ext2_filesystem_t *fs,
                                               uint32_t inode_index) {
  assert(inode_index != 0 && "Your are trying to access inode 0.");
  return (inode_index - 1) % fs->superblock.inodes_per_group;
}

/// @brief Determines which block contains our inode.
/// @param fs the ext2 filesystem structure.
/// @param inode_offset the inode offset inside the group.
/// @return which block contains our inode.
static uint32_t ext2_get_block_index_from_inode_offset(ext2_filesystem_t *fs,
                                                       uint32_t inode_offset) {
  return (inode_offset * fs->superblock.inode_size) / fs->block_size;
}

/// @brief Cheks if the bit at the given linear index is free.
/// @param buffer the buffer containing the bitmap
/// @param linear_index the linear index we want to check.
/// @return if the bit is 0 or 1.
/// @details
/// How we access the specific bits inside the bitmap takes inspiration from the
/// mailman's algorithm.
static ext2_block_status_t ext2_check_bitmap_bit(uint8_t *buffer,
                                                 uint32_t linear_index) {
  return (ext2_block_status_t)(bit_check(buffer[linear_index / 8],
                                         linear_index % 8) != 0);
}

/// @brief Sets the bit at the given linear index accordingly to `status`.
/// @param buffer the buffer containing the bitmap
/// @param linear_index the linear index we want to check.
/// @param status the new status of the block (free|occupied).
static void ext2_set_bitmap_bit(uint8_t *buffer, uint32_t linear_index,
                                ext2_block_status_t status) {
  if (status == ext2_block_status_occupied)
    bit_set_assign(buffer[linear_index / 8], linear_index % 8);
  else
    bit_clear_assign(buffer[linear_index / 8], linear_index % 8);
}

/// @brief Searches for a free inode inside the group data loaded inside the cache.
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param linear_index the output variable where we store the linear indes to the free inode.
/// @return true if we found a free inode, false otherwise.
static inline bool_t ext2_find_free_inode_in_group(ext2_filesystem_t *fs,
                                                   uint8_t *cache,
                                                   uint32_t *linear_index,
                                                   bool_t skip_reserved) {
  for ((*linear_index) = 0; (*linear_index) < fs->superblock.inodes_per_group;
       ++(*linear_index)) {
    // If we need to skip the reserved inodes, we skip the round if the
    // index is that of a reserved inode (superblock.first_ino).
    if (skip_reserved && ((*linear_index) < fs->superblock.first_ino))
      continue;
    // Check if the entry is free.
    if (!ext2_check_bitmap_bit(cache, *linear_index))
      return true;
  }
  return false;
}

/// @brief Searches for a free inode inside the Block Group Descriptor Table (BGDT).
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param group_index the output variable where we store the group index.
/// @param linear_index the output variable where we store the linear indes to the free inode.
/// @return true if we found a free inode, false otherwise.
static inline bool_t ext2_find_free_inode(ext2_filesystem_t *fs, uint8_t *cache,
                                          uint32_t *group_index,
                                          uint32_t *linear_index,
                                          uint32_t preferred_group) {
  // If we received a preference, try to find a free inode in that specific group.
  if (preferred_group != 0) {
    // Set the group index to the preferred group.
    (*group_index) = preferred_group;
    // Find the first free inode. We need to ask to skip reserved inodes,
    // only if we are in group 0.
    if (ext2_find_free_inode_in_group(fs, cache, linear_index,
                                      (*group_index) == 0))
      return true;
  }
  // Get the group and bit index of the first free block.
  for ((*group_index) = 0; (*group_index) < fs->block_groups_count;
       ++(*group_index)) {
    // Check if there are free inodes in this block group.
    if (fs->block_groups[(*group_index)].free_inodes_count > 0) {
      // Read the block bitmap.
      if (ext2_read_block(fs, fs->block_groups[(*group_index)].inode_bitmap,
                          cache) < 0) {
        dprintf("Failed to read the inode bitmap for group `%d`.\n",
                (*group_index));
        return false;
      }
      // Find the first free inode. We need to ask to skip reserved
      // inodes, only if we are in group 0.
      if (ext2_find_free_inode_in_group(fs, cache, linear_index,
                                        (*group_index) == 0))
        return true;
    }
  }
  return false;
}

/// @brief Searches for a free block inside the group data loaded inside the cache.
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param group_index the output variable where we store the group index.
/// @param linear_index the output variable where we store the linear indes to the free block.
/// @return true if we found a free block, false otherwise.
static inline bool_t ext2_find_free_block_in_group(ext2_filesystem_t *fs,
                                                   uint8_t *cache,
                                                   uint32_t *linear_index) {
  for ((*linear_index) = 0; (*linear_index) < fs->superblock.blocks_per_group;
       ++(*linear_index)) {
    // Check if the entry is free.
    if (!ext2_check_bitmap_bit(cache, *linear_index))
      return true;
  }
  return false;
}

/// @brief Searches for a free block.
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param linear_index the output variable where we store the linear indes to the free block.
/// @return true if we found a free block, false otherwise.
static inline bool_t ext2_find_free_block(ext2_filesystem_t *fs, uint8_t *cache,
                                          uint32_t *group_index,
                                          uint32_t *linear_index) {
  // Get the group and bit index of the first free block.
  for ((*group_index) = 0; (*group_index) < fs->block_groups_count;
       ++(*group_index)) {
    // Check if there are free blocks in this block group.
    if (fs->block_groups[(*group_index)].free_blocks_count > 0) {
      // Read the block bitmap.
      if (ext2_read_block(fs, fs->block_groups[(*group_index)].block_bitmap,
                          cache) < 0) {
        dprintf("Failed to read the block bitmap for group `%d`.\n",
                (*group_index));
        return false;
      }
      // Find the first free block.
      if (ext2_find_free_block_in_group(fs, cache, linear_index))
        return true;
    }
  }
  return false;
}

/// @brief Reads the superblock from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return the amount of data we read, or negative value for an error.
static int ext2_read_superblock(ext2_filesystem_t *fs) {
  dprintf("Read superblock for EXT2 filesystem (0x%x)\n", fs);
  return vfs_read(fs->block_device, &fs->superblock, 1024,
                  sizeof(ext2_superblock_t));
}

/// @brief Writes the superblock on the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return the amount of data we wrote, or negative value for an error.
static int ext2_write_superblock(ext2_filesystem_t *fs) {
  dprintf("Write superblock for EXT2 filesystem (0x%x)\n", fs);
  return vfs_write(fs->block_device, &fs->superblock, 1024,
                   sizeof(ext2_superblock_t));
}

/// @brief Read a block from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @param block_index the index of the block we want to read.
/// @param buffer the buffer where the content will be placed.
/// @return the amount of data we read, or negative value for an error.
static int ext2_read_block(ext2_filesystem_t *fs, uint32_t block_index,
                           uint8_t *buffer) {
  if (block_index == 0) {
    dprintf("You are trying to read an invalid block index (%d).\n",
            block_index);
    return -1;
  }
  if (buffer == NULL) {
    dprintf("You are trying to read with a NULL buffer.\n");
    return -1;
  }
  return vfs_read(fs->block_device, buffer, block_index * fs->block_size,
                  fs->block_size);
}

/// @brief Writes a block on the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @param block_index the index of the block we want to read.
/// @param buffer the buffer where the content will be placed.
/// @return the amount of data we wrote, or negative value for an error.
static int ext2_write_block(ext2_filesystem_t *fs, uint32_t block_index,
                            uint8_t *buffer) {
  if (block_index == 0) {
    dprintf("You are trying to write on an invalid block index (%d).\n",
            block_index);
    return -1;
  }
  if (buffer == NULL) {
    dprintf("You are trying to write with a NULL buffer.\n");
    return -1;
  }
  return vfs_write(fs->block_device, buffer, block_index * fs->block_size,
                   fs->block_size);
}

/// @brief Reads the Block Group Descriptor Table (BGDT) from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return 0 on success, -1 on failure.
static int ext2_read_bgdt(ext2_filesystem_t *fs) {
  dprintf("Read BGDT for EXT2 filesystem (0x%x)\n", fs);
  if (fs->block_groups) {
    for (uint32_t i = 0; i < fs->bgdt_length; ++i)
      ext2_read_block(
        fs, fs->bgdt_start_block + i,
        (uint8_t *)((uintptr_t)fs->block_groups + (fs->block_size * i)));
    return 0;
  }
  dprintf("The `block_groups` list is not initialized.\n");
  return -1;
}

/// @brief Writes the Block Group Descriptor Table (BGDT) to the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return 0 on success, -1 on failure.
static int ext2_write_bgdt(ext2_filesystem_t *fs) {
  dprintf("Write BGDT for EXT2 filesystem (0x%x)\n", fs);
  if (fs->block_groups) {
    for (uint32_t i = 0; i < fs->bgdt_length; ++i)
      ext2_write_block(
        fs, fs->bgdt_start_block + i,
        (uint8_t *)((uintptr_t)fs->block_groups + (fs->block_size * i)));
    return 0;
  }
  dprintf("The `block_groups` list is not initialized.\n");
  return -1;
}

/// @brief Reads an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @return 0 on success, -1 on failure.
static int ext2_read_inode(ext2_filesystem_t *fs, ext2_inode_t *inode,
                           uint32_t inode_index) {
  uint32_t group_index, block_index, inode_offset;
  if (inode_index == 0) {
    dprintf("You are trying to read an invalid inode index (%d).\n",
            inode_index);
    return -1;
  }
  // Retrieve the group index.
  group_index = ext2_get_group_index_from_inode(fs, inode_index);
  if (group_index > fs->block_groups_count) {
    dprintf("Invalid group index computed from inode index `%d`.\n",
            inode_index);
    return -1;
  }
  // Get the index of the inode inside the group.
  inode_offset = ext2_get_inode_offest_in_group(fs, inode_index);
  // Get the block offest.
  block_index = ext2_get_block_index_from_inode_offset(fs, inode_offset);
  // Get the real inode offset inside the block.
  inode_offset %= fs->inodes_per_block_count;
  // Log the address to the inode.
  dprintf("Read inode  (inode:%4u block:%4u offset:%4u)\n", inode_index,
          block_index, inode_offset);
  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);
  // Read the block containing the inode table.
  ext2_read_block(fs, fs->block_groups[group_index].inode_table + block_index,
                  cache);
  // Save the inode content.
  memcpy(inode,
         (ext2_inode_t *)((uintptr_t)cache +
                          (inode_offset * fs->superblock.inode_size)),
         sizeof(ext2_inode_t));
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return 0;
}

/// @brief Writes the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @return 0 on success, -1 on failure.
static int ext2_write_inode(ext2_filesystem_t *fs, ext2_inode_t *inode,
                            uint32_t inode_index) {
  uint32_t group_index, block_index, inode_offset;
  if (inode_index == 0) {
    dprintf("You are trying to read an invalid inode index (%d).\n",
            inode_index);
    return -1;
  }
  // Retrieve the group index.
  group_index = ext2_get_group_index_from_inode(fs, inode_index);
  if (group_index > fs->block_groups_count) {
    dprintf("Invalid group index computed from inode index `%d`.\n",
            inode_index);
    return -1;
  }
  // Get the offset of the inode inside the group.
  inode_offset = ext2_get_inode_offest_in_group(fs, inode_index);
  // Get the block offest.
  block_index = ext2_get_block_index_from_inode_offset(fs, inode_offset);
  // Get the real inode offset inside the block.
  inode_offset %= fs->inodes_per_block_count;
  // Log the address to the inode.
  dprintf("Write inode (inode:%4u block:%4u offset:%4u)\n", inode_index,
          block_index, inode_offset);
  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);
  // Read the block containing the inode table.
  ext2_read_block(fs, fs->block_groups[group_index].inode_table + block_index,
                  cache);
  // Write the inode.
  memcpy((ext2_inode_t *)((uintptr_t)cache +
                          (inode_offset * fs->superblock.inode_size)),
         inode, sizeof(ext2_inode_t));
  // Write back the block.
  ext2_write_block(fs, fs->block_groups[group_index].inode_table + block_index,
                   cache);
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return 0;
}

/// @brief Allocate a new inode.
/// @param fs the filesystem.
/// @param preferred_group the preferred group.
/// @return index of the inode.
/// @details
/// Here are the rules used to allocate new inodes:
///  - the inode for a new file is allocated in the same group of the inode of
///    its parent directory.
///  - inodes are allocated equally between groups.
static int ext2_allocate_inode(ext2_filesystem_t *fs,
                               unsigned preferred_group) {
  uint32_t group_index = 0, linear_index = 0, inode_index = 0;
  // Lock the filesystem.
  spinlock_lock(&fs->spinlock);
  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);
  // Search for a free inode.
  if (!ext2_find_free_inode(fs, cache, &group_index, &linear_index,
                            preferred_group)) {
    dprintf("Failed to find a free inode.\n");
    // Unlock the filesystem.
    spinlock_unlock(&fs->spinlock);
    // Free the cache.
    // kmem_cache_free(cache);
    kfree(cache);
    return 0;
  }
  // Compute the inode index.
  inode_index =
    (group_index * fs->superblock.inodes_per_group) + linear_index + 1U;
  // Set the inode as occupied.
  ext2_set_bitmap_bit(cache, linear_index, ext2_block_status_occupied);
  // Write back the inode bitmap.
  ext2_write_block(fs, fs->block_groups[group_index].inode_bitmap, cache);
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  // Reduce the number of free inodes.
  fs->block_groups[group_index].free_inodes_count -= 1;
  // Update the bgdt.
  ext2_write_bgdt(fs);
  // Reduce the number of inodes inside the superblock.
  fs->superblock.free_inodes_count -= 1;
  // Update the superblock.
  ext2_write_superblock(fs);
  // Unlock the filesystem.
  spinlock_unlock(&fs->spinlock);
  // Return the inode.
  return inode_index;
}

/// @brief Allocates a new block.
/// @param fs the filesystem.
/// @return 0 on failure, or the index of the new block on success.
static uint32_t ext2_allocate_block(ext2_filesystem_t *fs) {
  uint32_t group_index = 0, linear_index = 0, block_index = 0;
  // Lock the filesystem.
  spinlock_lock(&fs->spinlock);
  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);
  // Search for a free block.
  if (!ext2_find_free_block(fs, cache, &group_index, &linear_index)) {
    dprintf("Failed to find a free block.\n");
    // Unlock the filesystem.
    spinlock_unlock(&fs->spinlock);
    // Free the cache.
    // kmem_cache_free(cache);
    kfree(cache);
    return 0;
  }
  // Compute the block index.
  block_index = (group_index * fs->superblock.blocks_per_group) + linear_index;
  // Set the block as occupied.
  ext2_set_bitmap_bit(cache, linear_index, ext2_block_status_occupied);
  // Update the bitmap.
  ext2_write_block(fs, fs->block_groups[group_index].block_bitmap, cache);
  // Decrease the number of free blocks inside the BGDT entry.
  fs->block_groups[group_index].free_blocks_count -= 1;
  // Update the BGDT.
  ext2_write_bgdt(fs);
  // Decrease the number of free blocks inside the superblock.
  fs->superblock.free_blocks_count -= 1;
  // Update the superblock.
  ext2_write_superblock(fs);
  // Empty out the new block.
  memset(cache, 0, fs->block_size);
  ext2_write_block(fs, block_index, cache);
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  // Unlock the spinlock.
  spinlock_unlock(&fs->spinlock);
  return block_index;
}

/// @brief Sets the real block index based on the block index inside an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param block_index the block index inside the inode.
/// @param real_index the real block number.
/// @return 0 on success, a negative value on failure.
static int ext2_set_real_block_index(ext2_filesystem_t *fs, ext2_inode_t *inode,
                                     uint32_t inode_index, uint32_t block_index,
                                     uint32_t real_index) {
  // Set the direct block pointer.
  if (block_index < EXT2_INDIRECT_BLOCKS) {
    inode->data.blocks.dir_blocks[block_index] = real_index;
    return 0;
  }

  // Check if the index is among the indirect blocks.
  if (block_index < fs->indirect_blocks_index) {
    // Compute the indirect indices.
    uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;

    // Check that the indirect block points to a valid block.
    if (!inode->data.blocks.indir_block) {
      // Allocate a new block.
      uint32_t new_block_index = ext2_allocate_block(fs);
      if (new_block_index == 0)
        return -1;
      // Update the index.
      inode->data.blocks.indir_block = new_block_index;
      // Update the inode.
      if (ext2_write_inode(fs, inode, inode_index) == -1)
        return -1;
    }

    // Allocate the cache.
    // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
    // Clean the cache.
    memset(cache, 0, fs->block_size);
    // Read the indirect block (which contains pointers to the next set of blocks).
    ext2_read_block(fs, inode->data.blocks.indir_block, cache);
    // Write the index inside the final block.
    ((uint32_t *)cache)[a] = real_index;
    // Write back the indirect block.
    ext2_read_block(fs, inode->data.blocks.indir_block, cache);
    // Free the cache.
    // kmem_cache_free(cache);
    kfree(cache);
    return 0;
  }

  // For simplicity.
  uint32_t p1 = fs->pointers_per_block,
           p2 = fs->pointers_per_block * fs->pointers_per_block;

  // Check if the index is among the doubly-indirect blocks.
  if (block_index < fs->doubly_indirect_blocks_index) {
    // Compute the indirect indices.
    uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
    uint32_t b = a - p1;
    uint32_t c = b / p1;
    uint32_t d = b - (c * p1);

    // Check that the indirect block points to a valid block.
    if (!inode->data.blocks.doubly_indir_block) {
      // Allocate a new block.
      uint32_t new_block_index = ext2_allocate_block(fs);
      if (new_block_index == 0)
        return -1;
      // Update the index.
      inode->data.blocks.doubly_indir_block = new_block_index;
      // Update the inode.
      if (ext2_write_inode(fs, inode, inode_index) == -1)
        return -1;
    }

    // Allocate the cache.
    // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
    // Clean the cache.
    memset(cache, 0, fs->block_size);

    // Read the doubly-indirect block (which contains pointers to indirect blocks).
    ext2_read_block(fs, inode->data.blocks.doubly_indir_block, cache);
    // Check that the indirect block points to a valid block.
    if (!((uint32_t *)cache)[c]) {
      // Allocate a new block.
      uint32_t new_block_index = ext2_allocate_block(fs);
      if (new_block_index == 0) {
        // Free the cache.
        // kmem_cache_free(cache);
        kfree(cache);
        return -1;
      }
      // Update the index.
      ((uint32_t *)cache)[c] = new_block_index;
      // Write the doubly-indirect block back.
      ext2_write_block(fs, inode->data.blocks.doubly_indir_block, cache);
    }

    // Compute the index inside the indirect block.
    ext2_read_block(fs, ((uint32_t *)cache)[c], cache);
    // Write the index inside the final block.
    ((uint32_t *)cache)[d] = real_index;
    // Write back the indirect block.
    ext2_read_block(fs, ((uint32_t *)cache)[c], cache);
    // Free the cache.
    // kmem_cache_free(cache);
    kfree(cache);
    return 0;
  }

  // Check if the index is among the trebly-indirect blocks.
  if (block_index < fs->trebly_indirect_blocks_index) {
    // Compute the indirect indices.
    uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
    uint32_t b = a - p1;
    uint32_t c = b - p2;
    uint32_t d = c / p2;
    uint32_t e = c - (d * p2);
    uint32_t f = e / p1;
    uint32_t g = e - (f * p1);
    uint32_t block_index_save;

    // Check that the indirect block points to a valid block.
    if (!inode->data.blocks.trebly_indir_block) {
      // Allocate a new block.
      uint32_t new_block_index = ext2_allocate_block(fs);
      if (new_block_index == 0)
        return -1;
      // Update the index.
      inode->data.blocks.trebly_indir_block = new_block_index;
      // Update the inode.
      if (ext2_write_inode(fs, inode, inode_index) == -1)
        return -1;
    }

    // Allocate the cache.
    // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
    // Clean the cache.
    memset(cache, 0, fs->block_size);

    // Read the trebly-indirect block (which contains pointers to doubly-indirect blocks).
    ext2_read_block(fs, inode->data.blocks.trebly_indir_block, cache);
    // Check that the indirect block points to a valid block.
    if (!((uint32_t *)cache)[d]) {
      // Allocate a new block.
      uint32_t new_block_index = ext2_allocate_block(fs);
      if (new_block_index == 0) {
        // Free the cache.
        // kmem_cache_free(cache);
        kfree(cache);
        return -1;
      }
      // Update the index.
      ((uint32_t *)cache)[d] = new_block_index;
      // Write the doubly-indirect block back.
      ext2_write_block(fs, inode->data.blocks.trebly_indir_block, cache);
    }
    // Save the block index, otherwise with the next read we lose the block index list [d].
    block_index_save = ((uint32_t *)cache)[d];

    // Read the doubly-indirect block (which contains pointers to indirect blocks).
    ext2_read_block(fs, block_index_save, cache);
    // Check that the indirect block points to a valid block.
    if (!((uint32_t *)cache)[f]) {
      // Allocate a new block.
      uint32_t new_block_index = ext2_allocate_block(fs);
      if (new_block_index == 0) {
        // Free the cache.
        // kmem_cache_free(cache);
        kfree(cache);
        return -1;
      }
      // Update the index.
      ((uint32_t *)cache)[f] = new_block_index;
      // Write the doubly-indirect block back.
      ext2_write_block(fs, block_index_save, cache);
    }

    // Get the next group index.
    block_index_save = ((uint32_t *)cache)[f];
    // Read the indirect block (which contains pointers to the next set of blocks).
    ext2_read_block(fs, block_index_save, cache);
    // Write the index inside the final block.
    ((uint32_t *)cache)[g] = real_index;
    // Write back the indirect block.
    ext2_read_block(fs, block_index_save, cache);
    // Free the cache.
    // kmem_cache_free(cache);
    kfree(cache);
    return 0;
  }
  dprintf(
    "We failed to write the real block number of the block with index `%d`\n",
    block_index);
  return -1;
}

/// @brief Returns the real block index starting from a block index inside an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param block_index the block index inside the inode.
/// @return the real block number.
static uint32_t ext2_get_real_block_index(ext2_filesystem_t *fs,
                                          ext2_inode_t *inode,
                                          uint32_t block_index) {
  // Return the direct block pointer.
  if (block_index < EXT2_INDIRECT_BLOCKS) {
    return inode->data.blocks.dir_blocks[block_index];
  }
  // Create a variable for the real index.
  uint32_t real_index = 0;
  // For simplicity.
  uint32_t p1 = fs->pointers_per_block;
  uint32_t p2 = fs->pointers_per_block * fs->pointers_per_block;
  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);
  // Check if the index is among the indirect blocks.
  if (block_index < fs->indirect_blocks_index) {
    // Compute the indirect indices.
    uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
    // Read the indirect block (which contains pointers to the next set of blocks).
    ext2_read_block(fs, inode->data.blocks.indir_block, cache);
    // Compute the index inside the final block.
    real_index = ((uint32_t *)cache)[a];
  } else if (block_index < fs->doubly_indirect_blocks_index) {
    // The index is among the doubly-indirect blocks.
    // Compute the indirect indices.
    uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
    uint32_t b = a - p1;
    uint32_t c = b / p1;
    uint32_t d = b - (c * p1);
    // Read the doubly-indirect block (which contains pointers to indirect blocks).
    ext2_read_block(fs, inode->data.blocks.doubly_indir_block, cache);
    // Compute the index inside the indirect block.
    ext2_read_block(fs, ((uint32_t *)cache)[c], cache);
    // Compute the index inside the final block.
    real_index = ((uint32_t *)cache)[d];
  } else if (block_index < fs->trebly_indirect_blocks_index) {
    // The index is among the trebly-indirect blocks.
    // Compute the indirect indices.
    uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
    uint32_t b = a - p1;
    uint32_t c = b - p2;
    uint32_t d = c / p2;
    uint32_t e = c - (d * p2);
    uint32_t f = e / p1;
    uint32_t g = e - (f * p1);
    // Read the trebly-indirect block (which contains pointers to doubly-indirect blocks).
    ext2_read_block(fs, inode->data.blocks.trebly_indir_block, cache);
    // Read the doubly-indirect block (which contains pointers to indirect blocks).
    ext2_read_block(fs, ((uint32_t *)cache)[d], cache);
    // Read the indirect block (which contains pointers to the next set of blocks).
    ext2_read_block(fs, ((uint32_t *)cache)[f], cache);
    // Compute the index inside the final block.
    real_index = ((uint32_t *)cache)[g];
  } else {
    dprintf(
      "We failed to retrieve the real block number of the block with index `%d`\n",
      block_index);
  }
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  // Return the real index.
  return real_index;
}

/// @brief Allocate a new block for an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @param block_index The index of the block within the inode.
/// @return 0 on success, -1 on failure.
static int ext2_allocate_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode,
                                     uint32_t inode_index,
                                     uint32_t block_index) {
  dprintf("Allocating block with index `%d` for inode with index `%d`.\n",
          block_index, inode_index);
  // Allocate the block.
  int real_index = ext2_allocate_block(fs);
  if (real_index == -1)
    return -1;
  // Associate the real index and the index inside the inode.
  if (ext2_set_real_block_index(fs, inode, inode_index, block_index,
                                real_index) == -1)
    return -1;
  // Compute the new blocks count.
  uint32_t blocks_count = (block_index + 1) * fs->blocks_per_block_count;
  if (inode->blocks_count < blocks_count) {
    // Set the blocks count.
    inode->blocks_count = blocks_count;
    // Update the size.
    inode->size = (blocks_count / fs->blocks_per_block_count) * fs->block_size;
    dprintf("Setting the block count for inode `%d` to `%d` blocks.\n",
            inode_index, blocks_count / fs->blocks_per_block_count);
  }
  // Update the inode.
  if (ext2_write_inode(fs, inode, inode_index) == -1)
    return -1;
  return 0;
}

/// @brief Reads the real block starting from an inode and the block index inside the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param block_index the index of the block within the inode.
/// @param buffer the buffer where to put the data.
/// @return the amount of data we read, or negative value for an error.
static ssize_t ext2_read_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode,
                                     uint32_t block_index, uint8_t *buffer) {
  if (block_index >= (inode->blocks_count / fs->blocks_per_block_count))
    return -1;
  // Get the real index.
  uint32_t real_index = ext2_get_real_block_index(fs, inode, block_index);
  if (real_index == 0)
    return -1;
  // Log the address to the inode block.
  dprintf("Read inode block  (block:%4u real:%4u)\n", block_index, real_index);
  // Read the block.
  return ext2_read_block(fs, real_index, buffer);
}

/// @brief Writes the real block starting from an inode and the block index inside the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @param block_index the index of the block within the inode.
/// @param buffer the buffer where to put the data.
/// @return the amount of data we wrote, or negative value for an error.
static ssize_t ext2_write_inode_block(ext2_filesystem_t *fs,
                                      ext2_inode_t *inode, uint32_t inode_index,
                                      uint32_t block_index, uint8_t *buffer) {
  while (block_index >= (inode->blocks_count / fs->blocks_per_block_count)) {
    ext2_allocate_inode_block(
      fs, inode, inode_index,
      (inode->blocks_count / fs->blocks_per_block_count));
    ext2_write_inode(fs, inode, inode_index);
  }
  // Get the real index.
  uint32_t real_index = ext2_get_real_block_index(fs, inode, block_index);
  if (real_index == 0)
    return -1;
  // Log the address to the inode block.
  dprintf("Write inode block (block:%4u real:%4u inode:%4u)\n", block_index,
          real_index, inode_index);
  // Write the block.
  return ext2_write_block(fs, real_index, buffer);
}

/// @brief Reads the data from the given inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index the index of the inode.
/// @param offset the offset from which we start reading the data.
/// @param nbyte the number of bytes to read.
/// @param buffer the buffer containing the data.
/// @return the amount we read.
static ssize_t ext2_read_inode_data(ext2_filesystem_t *fs, ext2_inode_t *inode,
                                    uint32_t inode_index, off_t offset,
                                    size_t nbyte, char *buffer) {
  // Check if the file is empty.
  if (inode->size == 0)
    return 0;

  uint32_t end;
  if ((offset + nbyte) > inode->size) {
    end = inode->size;
  } else {
    end = offset + nbyte;
  }
  uint32_t start_block  = offset / fs->block_size;
  uint32_t end_block    = end / fs->block_size;
  uint32_t end_size     = end - end_block * fs->block_size;
  uint32_t size_to_read = end - offset;

  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);

  if (start_block == end_block) {
    // Read the real block.
    if (ext2_read_inode_block(fs, inode, start_block, cache) == -1) {
      dprintf("Failed to read the inode block `%d`\n", start_block);
      goto free_cache_return_error;
    }
    // Copy the content back to the buffer.
    memcpy(
      buffer,
      (uint8_t *)(((uintptr_t)cache) + ((uintptr_t)offset % fs->block_size)),
      size_to_read);
  } else {
    uint32_t block_offset;
    uint32_t blocks_read = 0;
    for (block_offset = start_block; block_offset < end_block;
         block_offset++, blocks_read++) {
      // Read the real block.
      if (ext2_read_inode_block(fs, inode, block_offset, cache) == -1) {
        dprintf("Failed to read the inode block `%d`\n", block_offset);
        goto free_cache_return_error;
      }
      // Copy the content back to the buffer.
      if (block_offset == start_block) {
        memcpy(buffer,
               (uint8_t *)(((uintptr_t)cache) +
                           ((uintptr_t)offset % fs->block_size)),
               fs->block_size - (offset % fs->block_size));
      } else {
        memcpy(buffer + fs->block_size * blocks_read -
                 (offset % fs->block_size),
               cache, fs->block_size);
      }
    }
    if (end_size) {
      // Read the real block.
      if (ext2_read_inode_block(fs, inode, end_block, cache) == -1) {
        dprintf("Failed to read the inode block `%d`\n", block_offset);
        goto free_cache_return_error;
      }
      // Copy the content back to the buffer.
      memcpy(buffer + fs->block_size * blocks_read - (offset % fs->block_size),
             cache, end_size);
    }
  }
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return size_to_read;
free_cache_return_error:
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return -1;
}

/// @brief Writes the data on the given inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index the index of the inode.
/// @param offset the offset from which we start writing the data.
/// @param nbyte the number of bytes to write.
/// @param buffer the buffer containing the data.
/// @return the amount written.
static ssize_t ext2_write_inode_data(ext2_filesystem_t *fs, ext2_inode_t *inode,
                                     uint32_t inode_index, off_t offset,
                                     size_t nbyte, char *buffer) {
  uint32_t end = offset + nbyte;
  if (end > inode->size) {
    inode->size = end;
  }
  uint32_t start_block   = offset / fs->block_size;
  uint32_t end_block     = end / fs->block_size;
  uint32_t end_size      = end - end_block * fs->block_size;
  uint32_t size_to_write = end - offset;

  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);

  if (start_block == end_block) {
    // Read the real block, if we fail is not a problem.
    ext2_read_inode_block(fs, inode, start_block, cache);
    // Copy the content back to the buffer.
    memcpy(
      (uint8_t *)(((uintptr_t)cache) + ((uintptr_t)offset % fs->block_size)),
      buffer, size_to_write);
    // Write the block back.
    if (!ext2_write_inode_block(fs, inode, inode_index, start_block, cache)) {
      dprintf("Failed to write the inode block `%d`\n", start_block);
      goto free_cache_return_error;
    }
  } else {
    uint32_t block_offset, blocks_read = 0;
    for (block_offset = start_block; block_offset < end_block;
         ++block_offset, ++blocks_read) {
      // Read the real block, if we fail is not a problem.
      ext2_read_inode_block(fs, inode, block_offset, cache);
      if (block_offset == start_block) {
        // Copy the content back to the buffer.
        memcpy((uint8_t *)(((uintptr_t)cache) +
                           ((uintptr_t)offset % fs->block_size)),
               buffer, fs->block_size - (offset % fs->block_size));
      } else {
        // Copy the content back to the buffer.
        memcpy(cache,
               buffer + fs->block_size * blocks_read -
                 (offset % fs->block_size),
               fs->block_size);
      }
      // Write the block back.
      if (!ext2_write_inode_block(fs, inode, inode_index, block_offset,
                                  cache)) {
        dprintf("Failed to write the inode block `%d`\n", start_block);
        goto free_cache_return_error;
      }
    }
    if (end_size) {
      // Read the real block, if we fail is not a problem.
      ext2_read_inode_block(fs, inode, end_block, cache);
      // Copy the content back to the buffer.
      memcpy(cache,
             buffer + fs->block_size * blocks_read - (offset % fs->block_size),
             end_size);
      // Write the block back.
      if (ext2_write_inode_block(fs, inode, inode_index, end_block, cache)) {
        dprintf("Failed to write the inode block `%d`\n", start_block);
        goto free_cache_return_error;
      }
    }
  }
  return size_to_write;
free_cache_return_error:
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return -1;
}

// ============================================================================
// Directory Entry Iteration Functions
// ============================================================================

/// @brief Iterator for visiting the directory entries.
typedef struct ext2_direntry_iterator_t {
  ext2_filesystem_t *fs; ///< A pointer to the filesystem.
  uint8_t *cache;        ///< Cache used for reading.
  ext2_inode_t *inode;   ///< A pointer to the directory inode.
  uint32_t block_index;  ///< The current block we are reading.
  uint32_t total_offset; ///< The total amount of bytes we have read.
  uint32_t
    block_offset; ///< The total amount of bytes we have read inside the current block.
  ext2_dirent_t *direntry; ///< Pointer to the directory entry.
} ext2_direntry_iterator_t;

/// @brief Returns the ext2_dirent_t pointed by the iterator.
/// @param iterator the iterator.
/// @return pointer to the ext2_dirent_t
ext2_dirent_t *ext2_direntry_iterator_get(ext2_direntry_iterator_t *iterator) {
  return (ext2_dirent_t *)((uintptr_t)iterator->cache + iterator->block_offset);
}

/// @brief Check if the iterator is valid.
/// @param iterator the iterator to check.
/// @return true if valid, false otherwise.
bool_t ext2_direntry_iterator_valid(ext2_direntry_iterator_t *iterator) {
  return iterator->direntry != NULL;
}

/// @brief Initializes the iterator and reads the first block.
/// @param fs pointer to the filesystem.
/// @param cache used for reading.
/// @param inode pointer to the directory inode.
/// @return The initialized directory iterator.
ext2_direntry_iterator_t ext2_direntry_iterator_begin(ext2_filesystem_t *fs,
                                                      uint8_t *cache,
                                                      ext2_inode_t *inode) {
  ext2_direntry_iterator_t it = { .fs           = fs,
                                  .cache        = cache,
                                  .inode        = inode,
                                  .block_index  = 0,
                                  .total_offset = 0,
                                  .block_offset = 0,
                                  .direntry     = NULL };
  // Start by reading the first block of the inode.
  if (ext2_read_inode_block(fs, inode, it.block_index, cache) == -1) {
    dprintf("Failed to read the inode block `%d`\n", it.block_index);
  } else {
    // Initialize the directory entry.
    it.direntry = ext2_direntry_iterator_get(&it);
  }
  return it;
}

/// @brief Moves to the next direntry, and moves to the next block if necessary.
/// @param iterator the iterator.
void ext2_direntry_iterator_next(ext2_direntry_iterator_t *iterator) {
  // Get the current rec_len.
  uint32_t rec_len = ext2_direntry_iterator_get(iterator)->rec_len;
  // Advance the offsets.
  iterator->block_offset += rec_len;
  iterator->total_offset += rec_len;
  // If we reached the end of the inode, stop.
  if (iterator->total_offset >= iterator->inode->size) {
    // The iterator is not valid anymore.
    iterator->direntry = NULL;
    return;
  }
  // If we exceed the size of a block, move to the next block.
  if (iterator->block_offset >= iterator->fs->block_size) {
    // Increase the block index.
    iterator->block_index += 1;
    // Remove the exceeding size, so that we start correctly in the new block.
    iterator->block_offset -= iterator->fs->block_size;
    // Read the new block.
    if (ext2_read_inode_block(iterator->fs, iterator->inode,
                              iterator->block_index, iterator->cache) == -1) {
      dprintf("Failed to read the inode block `%d`\n", iterator->block_index);
      // The iterator is not valid anymore.
      iterator->direntry = NULL;
      return;
    }
  }
  // Read the direntry.
  iterator->direntry = ext2_direntry_iterator_get(iterator);
}

static inline bool_t ext2_directory_is_empty(ext2_filesystem_t *fs,
                                             uint8_t *cache,
                                             ext2_inode_t *inode) {
  ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, inode);
  for (; ext2_direntry_iterator_valid(&it); ext2_direntry_iterator_next(&it)) {
    if (it.direntry->inode != 0)
      return false;
  }
  return true;
}

// ============================================================================
// Directory Entry Management Functions
// ============================================================================

static inline uint32_t ext2_get_rec_len_from_name(const char *name) {
  unsigned int rec_len = sizeof(ext2_dirent_t) + strlen(name) - EXT2_NAME_LEN;
  rec_len += (rec_len % 4) ? (4 - (rec_len % 4)) : 0;
  return rec_len;
}

static inline uint32_t
ext2_get_rec_len_from_direntry(const ext2_dirent_t *direntry) {
  unsigned int rec_len =
    sizeof(ext2_dirent_t) + direntry->name_len - EXT2_NAME_LEN;
  rec_len += (rec_len % 4) ? (4 - (rec_len % 4)) : 0;
  return rec_len;
}

static int ext2_allocate_direntry(ext2_filesystem_t *fs,
                                  uint32_t parent_inode_index,
                                  uint32_t inode_index, const char *name,
                                  uint8_t file_type) {
  // Get the inode associated with the new directory entry.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, inode_index) == -1) {
    dprintf("Failed to read the inode of the directory entry (%d).\n",
            inode_index);
    return -1;
  }
  // Update the number of links to the inode.
  inode.links_count += 1;
  // Write the inode back.
  if (ext2_write_inode(fs, &inode, inode_index) == -1) {
    dprintf("Failed to update the inode of the directory entry.\n");
    return -1;
  }
  // Get the inode associated with the parent directory.
  ext2_inode_t parent_inode;
  if (ext2_read_inode(fs, &parent_inode, parent_inode_index) == -1) {
    dprintf("Failed to read the parent inode (%d).\n", parent_inode_index);
    return -1;
  }
  // Check that the parent is a directory.
  if (!bitmask_check(parent_inode.mode, EXT2_S_IFDIR)) {
    dprintf("The parent inode is not a directory (ino: %d, mode: %d).\n",
            parent_inode_index, parent_inode.mode);
    return -1;
  }
  dprintf("ext2_allocate_direntry(parent: %d, name: \"%s\", inode: %d)\n",
          parent_inode_index, name, inode_index);
  // Compute the rec_len for the name of the new direntry. Remember, the name
  // is not actually 256 chars long as specified in EXT2_NAME_LEN, that is
  // just a maximum.
  unsigned int rec_len = ext2_get_rec_len_from_name(name);

  dprintf("    Our directory entry looks like this:\n");
  dprintf("        inode     = %d\n", inode_index);
  dprintf("        rec_len   = %d\n", rec_len);
  dprintf("        name      = %s (%d)\n", name, strlen(name));
  dprintf("        file_type = %d (vfs: %d)\n", EXT2_S_IFREG, DT_REG);

  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);
  // Save the previous direntry.
  ext2_dirent_t *previous = NULL;
  // Iterate the directory entries.
  ext2_direntry_iterator_t it =
    ext2_direntry_iterator_begin(fs, cache, &parent_inode);
  for (; ext2_direntry_iterator_valid(&it); ext2_direntry_iterator_next(&it)) {
    // If we hit a direntry with an empty inode, that is a free direntry.
    if (it.direntry->inode == 0) {
      dprintf("Found free direntry: %p (%d <= %d)\n", it.direntry,
              it.direntry->rec_len, rec_len);
      if (rec_len <= it.direntry->rec_len)
        break;
    }
    // Compute the real rec_len of the entry.
    uint32_t real_rec_len = ext2_get_rec_len_from_direntry(it.direntry);
    // If the previous direntry has a wrong rec_len (wrong because it identifies the last).
    if ((it.direntry->rec_len != real_rec_len) &&
        (it.total_offset + it.direntry->rec_len == parent_inode.size)) {
      // Set the previous pointer.
      previous = it.direntry;
      // Fix the rec_len of the entry.
      it.direntry->rec_len = real_rec_len;
      // Move the block offset correctly.
      it.block_offset += real_rec_len;
      // Move the total offset correctly.
      it.total_offset += real_rec_len;
      // Clean the pointer to the direntry inside the iterator.
      it.direntry = NULL;
      // Stop here.
      break;
    }
  }
  if (it.direntry) {
    dprintf("    We need to replace the direntry: %p\n", it.direntry);
    // Clean the previous name.
    memset(it.direntry->name, 0, it.direntry->name_len);
    // Set the inode.
    it.direntry->inode = inode_index;
    // Set the new name length,
    it.direntry->name_len = strlen(name);
    // Set the new name.
    memcpy(it.direntry->name, name, it.direntry->name_len);
    // Set the file type.
    it.direntry->file_type = file_type;

    if (ext2_write_inode_block(fs, &parent_inode, parent_inode_index,
                               it.block_index, cache) == -1) {
      dprintf("Failed to update the block of the father directory.\n");
      goto free_cache_return_error;
    }
    goto free_cache_return_success;

  } else if ((it.block_offset + rec_len) >= fs->block_size) {
    dprintf("    We need a new direntry, and a new block (%d + %d >= %d).\n",
            it.block_offset, rec_len, fs->block_size);
    it.block_index += 1;
    if (ext2_allocate_inode_block(fs, &parent_inode, parent_inode_index,
                                  it.block_index) == -1) {
      dprintf("Failed to allocate a new block for an inode.\n");
      goto free_cache_return_error;
    }
    it.block_offset = 0;
    parent_inode.size += fs->block_size;
    if (ext2_write_inode(fs, &parent_inode, parent_inode_index) == -1) {
      dprintf("Failed to update the inode of the father directory.\n");
      goto free_cache_return_error;
    }
  } else if (previous) {
    dprintf("    We need a new direntry, from the same block (after \"%s\").\n",
            previous->name);
  }
  dprintf("    total_offset = %d\n", it.total_offset);
  dprintf("    block_offset = %d\n", it.block_offset);

  ext2_dirent_t *new_direntry =
    (ext2_dirent_t *)((uintptr_t)cache + it.block_offset);

  new_direntry->inode     = inode_index;
  new_direntry->rec_len   = fs->block_size - it.block_offset;
  new_direntry->name_len  = strlen(name);
  new_direntry->file_type = file_type;
  memcpy(new_direntry->name, name, strlen(name));

  if (ext2_write_inode_block(fs, &parent_inode, parent_inode_index,
                             it.block_index, cache) == -1) {
    dprintf("Failed to update the block of the father directory.\n");
    goto free_cache_return_error;
  }

free_cache_return_success:
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return 0;

free_cache_return_error:
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return -1;
}

/// @brief Finds the entry with the given `name` inside the `directory`.
/// @param directory the directory in which we perform the search.
/// @param name the name of the entry we are looking for.
/// @param search the output variable where we save the info about the entry.
/// @return 0 on success, -1 on failure.
static int ext2_find_direntry(ext2_filesystem_t *fs, ino_t ino,
                              const char *name,
                              ext2_direntry_search_t *search) {
  if (fs == NULL) {
    dprintf("You provided a NULL filesystem.\n");
    return -1;
  }
  if (name == NULL) {
    dprintf("You provided a NULL name.\n");
    return -1;
  }
  if (search == NULL) {
    dprintf("You provided a NULL search.\n");
    return -1;
  }
  if (search->direntry == NULL) {
    dprintf("You provided a NULL direntry.\n");
    return -1;
  }
  //dprintf("ext2_find_direntry(ino: %d, name: \"%s\")\n", ino, name);
  // Get the inode associated with the file.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, ino) == -1) {
    dprintf("Failed to read the inode (%d).\n", ino);
    return -1;
  }
  // Check that the parent is a directory.
  if (!bitmask_check(inode.mode, EXT2_S_IFDIR)) {
    dprintf("The parent inode is not a directory (ino: %d, mode: %d).\n", ino,
            inode.mode);
    return -1;
  }
  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);
  ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, &inode);
  for (; ext2_direntry_iterator_valid(&it); ext2_direntry_iterator_next(&it)) {
    // Skip unused inode.
    if (it.direntry->inode == 0)
      continue;
    // Chehck the name.
    if (!strcmp(it.direntry->name, ".") && !strcmp(name, "/")) {
      break;
    }
    // Check if the entry has the same name.
    if (strlen(name) == it.direntry->name_len)
      if (!strncmp(it.direntry->name, name, it.direntry->name_len))
        break;
  }
  // Copy the inode of the parent, even if we did not find the entry.
  search->parent_inode = ino;
  // Check if we have found the entry.
  if (it.direntry == NULL)
    goto free_cache_return_error;
  // Copy the direntry.
  memcpy(search->direntry, it.direntry, sizeof(ext2_dirent_t));
  // Close the name.
  search->direntry->name[search->direntry->name_len] = 0;
  // Copy the index of the block containing the direntry.
  search->block_index = it.block_index;
  // Copy the offset of the direntry inside the block.
  search->block_offset = it.block_offset;
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);

  //dprintf("ext2_find_direntry(ino: %d, name: \"%s\") -> (ino: %d, name: \"%s\")\n",
  //         ino, name, search->direntry->inode, search->direntry->name);
  return 0;
free_cache_return_error:
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return -1;
}

/// @brief Finds the entry with the given `name` inside the `directory`.
/// @param directory the directory in which we perform the search.
/// @param name the name of the entry we are looking for.
/// @param search the output variable where we save the info about the entry.
/// @return 0 on success, -1 on failure.
static int ext2_find_direntry_simple(ext2_filesystem_t *fs, ino_t ino,
                                     const char *name,
                                     ext2_dirent_t *direntry) {
  // Prepare the structure for the search.
  ext2_direntry_search_t search = {
    .direntry = direntry, .block_index = 0, .block_offset = 0, .parent_inode = 0
  };
  return ext2_find_direntry(fs, ino, name, &search);
}

/// @brief Searches the entry specified in `path` starting from `directory`.
/// @param directory the directory from which we start performing the search.
/// @param path the path of the entry we are looking for, it cna be a relative path.
/// @param search the output variable where we save the entry information.
/// @return 0 on success, -1 on failure.
static int ext2_resolve_path(vfs_file_t *directory, char *path,
                             ext2_direntry_search_t *search) {
  // Check the pointers.
  if (directory == NULL) {
    dprintf("You provided a NULL directory.\n");
    return -1;
  }
  if (path == NULL) {
    dprintf("You provided a NULL path.\n");
    return -1;
  }
  if (search == NULL) {
    dprintf("You provided a NULL search.\n");
    return -1;
  }
  if (search->direntry == NULL) {
    dprintf("You provided a NULL direntry.\n");
    return -1;
  }
  // Get the filesystem.
  ext2_filesystem_t *fs = (ext2_filesystem_t *)directory->device;
  if (fs == NULL) {
    dprintf("The file does not belong to an EXT2 filesystem `%s`.\n",
            directory->name);
    return -1;
  }
  // If the path is `/`.
  if (strcmp(path, "/") == 0)
    return ext2_find_direntry(fs, directory->ino, path, search);
  ino_t ino      = directory->ino;
  char *tmp_path = strdup(path);
  char *token    = strtok(tmp_path, "/");
  while (token) {
    if (!ext2_find_direntry(fs, ino, token, search)) {
      ino = search->direntry->inode;
    } else {
      memset(search->direntry, 0, sizeof(ext2_dirent_t));
      kfree(tmp_path);
      return -1;
    }
    token = strtok(NULL, "/");
  }
  kfree(tmp_path);
  return 0;
}

/// @brief Searches the entry specified in `path` starting from `directory`.
/// @param directory the directory from which we start performing the search.
/// @param path the path of the entry we are looking for, it cna be a relative path.
/// @param direntry the output variable where we save the found entry.
/// @return 0 on success, -1 on failure.
static int ext2_resolve_path_direntry(vfs_file_t *directory, char *path,
                                      ext2_dirent_t *direntry) {
  // Check the pointers.
  if (directory == NULL) {
    dprintf("You provided a NULL directory.\n");
    return -1;
  }
  if (path == NULL) {
    dprintf("You provided a NULL path.\n");
    return -1;
  }
  if (direntry == NULL) {
    dprintf("You provided a NULL direntry.\n");
    return -1;
  }
  // Prepare the structure for the search.
  ext2_direntry_search_t search;
  memset(&search, 0, sizeof(ext2_direntry_search_t));
  // Initialize the search structure.
  search.direntry     = direntry;
  search.block_index  = 0;
  search.block_offset = 0;
  search.parent_inode = 0;
  return ext2_resolve_path(directory, path, &search);
}

/// @brief Get the ext2 filesystem object starting from a path.
/// @param absolute_path the absolute path for which we want to find the associated EXT2 filesystem.
/// @return a pointer to the EXT2 filesystem, NULL otherwise.
static ext2_filesystem_t *get_ext2_filesystem(const char *absolute_path) {
  if (absolute_path == NULL) {
    dprintf("We received a NULL absolute path.\n");
    return NULL;
  }
  if (absolute_path[0] != '/') {
    dprintf("We did not received an absolute path `%s`.\n", absolute_path);
    return NULL;
  }
  super_block_t *sb = vfs_get_superblock(absolute_path);
  if (sb == NULL) {
    dprintf("Cannot find the superblock for the absolute path `%s`.\n",
            absolute_path);
    return NULL;
  }
  vfs_file_t *sb_root = sb->root;
  if (sb_root == NULL) {
    dprintf("Cannot find the superblock root for the absolute path `%s`.\n",
            absolute_path);
    return NULL;
  }
  // Get the filesystem.
  ext2_filesystem_t *fs = (ext2_filesystem_t *)sb_root->device;
  if (fs == NULL) {
    dprintf("The file does not belong to an EXT2 filesystem `%s`.\n",
            sb_root->name);
    return NULL;
  }
  // Check the magic number.
  if (fs->superblock.magic != EXT2_SUPERBLOCK_MAGIC) {
    dprintf("The file does not belong to an EXT2 filesystem `%s`.\n",
            sb_root->name);
    return NULL;
  }
  return fs;
}

static int ext2_init_vfs_file(ext2_filesystem_t *fs, vfs_file_t *file,
                              ext2_inode_t *inode, uint32_t inode_index,
                              const char *name, size_t name_len) {
  // Copy the name
  memcpy(file->name, name, name_len);
  file->name[name_len] = 0;
  // Set the device.
  file->device = (void *)fs;
  // Set the mask.
  file->mask = inode->mode & 0xFFF;
  // Set UID and GID.
  file->uid = inode->uid;
  file->gid = inode->gid;
  // Set the VFS specific flags.
  file->flags = 0;
  if ((inode->mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
    file->flags |= DT_REG;
  }
  if ((inode->mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
    file->flags |= DT_DIR;
  }
  if ((inode->mode & EXT2_S_IFBLK) == EXT2_S_IFBLK) {
    file->flags |= DT_BLK;
  }
  if ((inode->mode & EXT2_S_IFCHR) == EXT2_S_IFCHR) {
    file->flags |= DT_CHR;
  }
  if ((inode->mode & EXT2_S_IFIFO) == EXT2_S_IFIFO) {
    file->flags |= DT_FIFO;
  }
  if ((inode->mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
    file->flags |= DT_LNK;
  }
  // Set the inode.
  file->ino = inode_index;
  // Set the size of the file.
  file->length = inode->size;
  //uint32_t impl;
  //uint32_t open_flags;
  // Set the open count.
  file->count = 0;
  // Set the timing information.
  file->atime = inode->atime;
  file->mtime = inode->mtime;
  file->ctime = inode->ctime;
  // Set the FS specific operations.
  file->sys_operations = &ext2_sys_operations;
  file->fs_operations  = &ext2_fs_operations;
  // Set the read offest.
  file->f_pos = 0;
  // Set the number of links.
  file->nlink = inode->links_count;
  // Initialize the list of siblings.
  list_head_init(&file->siblings);
  // Set the refcount to zero.
  file->refcount = 0;
  return 0;
}

static vfs_file_t *ext2_find_vfs_file_with_inode(ext2_filesystem_t *fs,
                                                 ino_t inode) {
  vfs_file_t *file = NULL;
  if (!list_head_empty(&fs->opened_files)) {
    list_for_each_decl(it, &fs->opened_files) {
      // Get the file structure.
      file = list_entry(it, vfs_file_t, siblings);
      if (file && (file->ino == inode))
        return file;
    }
  }
  return NULL;
}

// ============================================================================
// Virtual FileSystem (VFS) Functions
// ============================================================================

/// @brief Creates and initializes a new inode.
/// @param fs the filesystem.
/// @param inode the inode we use to initialize the root of the filesystem.
/// @param preferred_group the preferred group where the inode should be allocated.
/// @return the inode index on success, -1 on failure.
static int ext2_create_inode(ext2_filesystem_t *fs, ext2_inode_t *inode,
                             mode_t mode, uint32_t preferred_group) {
  if (fs == NULL) {
    dprintf("Received a null EXT2 filesystem.\n");
    return -1;
  }
  if (inode == NULL) {
    dprintf("Received a null EXT2 inode.\n");
    return -1;
  }
  task_struct *task = scheduler_get_current_process();
  if (task == NULL) {
    dprintf("Failed to get the current running process.\n");
    return -1;
  }
  // Allocate an inode, inside the preferred_group if possible.
  int inode_index = ext2_allocate_inode(fs, preferred_group);
  if (inode_index == 0) {
    dprintf("Failed to allocate a new inode.\n");
    return -1;
  }
  // Clean the inode structure.
  memset(inode, 0, sizeof(ext2_inode_t));
  // Get the inode associated with the directory entry.
  if (ext2_read_inode(fs, inode, inode_index) == -1) {
    dprintf("Failed to read the newly created inode.\n");
    return -1;
  }
  // Set the inode mode.
  inode->mode = mode;
  // Set the user identifiers of the owners.
  inode->uid = task->uid;
  // Set the size of the file in bytes.
  inode->size = 0;
  // Set the time that the inode was accessed.
  inode->atime = sys_time(NULL);
  // Set the time that the inode was created.
  inode->ctime = inode->atime;
  // Set the time that the inode was modified the last time.
  inode->mtime = inode->atime;
  // Set the time that the inode was deleted.
  inode->dtime = 0;
  // Set the group identifiers of the owners.
  inode->gid = task->gid;
  // Set the number of hard links.
  inode->links_count = 0;
  // Set the blocks count.
  inode->blocks_count = 0;
  // Set the file flags.
  inode->flags = 0;
  // Set the OS dependant value.
  inode->osd1 = 0;
  // Set the blocks data.
  memset(&inode->data, 0, sizeof(inode->data));
  // Set the value used to indicate the file version (used by NFS).
  inode->generation = 0;
  // TODO: The value indicating the block number containing the extended attributes.
  inode->file_acl = 0;
  // TODO: For regular files this 32bit value contains the high 32 bits of the 64bit file size.
  inode->dir_acl = 0;
  // TODO:Value indicating the location of the file fragment.
  inode->fragment_addr = 0;
  // TODO: OS dependant structure.
  memset(&inode->osd2, 0, sizeof(inode->osd2));
  return inode_index;
}

/// @brief Creates a new file or rewrite an existing one.
/// @param path path to the file.
/// @param mode mode for file creation.
/// @return file descriptor number, -1 otherwise and errno is set to indicate the error.
/// @details
/// It is equivalent to: open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)
static vfs_file_t *ext2_creat(const char *path, mode_t permission) {
  // Get the name of the directory.
  const char *parent_path = dirname(path), *file_name = basename(path);
  if (strcmp(parent_path, path) == 0) {
    return NULL;
  }
  // Get the parent VFS node.
  vfs_file_t *parent = vfs_open(parent_path, O_RDONLY, 0);
  if (parent == NULL) {
    errno = ENOENT;
    return NULL;
  }
  // flags = O_WRONLY | O_CREAT | O_TRUNC
  // Get the filesystem.
  ext2_filesystem_t *fs = (ext2_filesystem_t *)parent->device;
  if (fs == NULL) {
    dprintf("The parent does not belong to an EXT2 filesystem `%s`.\n",
            parent->name);
    goto close_parent_return_null;
  }
  // Prepare the structure for the direntry.
  ext2_dirent_t direntry;
  // Prepare an inode, it will come in handy either way.
  ext2_inode_t inode;
  // Search if the entry already exists.
  if (!ext2_find_direntry_simple(fs, parent->ino, file_name, &direntry)) {
    if (ext2_read_inode(fs, &inode, direntry.inode) == -1) {
      dprintf("Failed to read the inode of `%s`.\n", direntry.name);
      goto close_parent_return_null;
    }
    vfs_file_t *file = ext2_find_vfs_file_with_inode(fs, direntry.inode);
    if (file == NULL) {
      // Allocate the memory for the file.
      // file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
      file = kmalloc(sizeof(vfs_file_t));
      if (file == NULL) {
        dprintf("Failed to allocate memory for the EXT2 file.\n");
        goto close_parent_return_null;
      }
      if (ext2_init_vfs_file(fs, file, &inode, direntry.inode, direntry.name,
                             direntry.name_len) == -1) {
        dprintf("Failed to properly set the VFS file.\n");
        goto close_parent_return_null;
      }
      // Add the vfs_file to the list of associated files.
      list_head_insert_before(&file->siblings, &fs->opened_files);
    }
    return file;
  }
  // Set the inode mode.
  uint32_t mode = EXT2_S_IFREG;
  mode |= 0xFFF & permission;
  // Get the group index of the parent.
  uint32_t group_index = ext2_get_group_index_from_inode(fs, parent->ino);
  // Create and initialize the new inode.
  int inode_index = ext2_create_inode(fs, &inode, mode, group_index);
  if (inode_index == -1) {
    dprintf("Failed to create a new inode inside `%s` (group index: %d).\n",
            parent->name, group_index);
    goto close_parent_return_null;
  }
  // Write the inode.
  if (ext2_write_inode(fs, &inode, inode_index) == -1) {
    dprintf("Failed to write the newly created inode.\n");
    goto close_parent_return_null;
  }
  // Initialize the file.
  if (ext2_allocate_direntry(fs, parent->ino, inode_index, file_name,
                             ext2_file_type_regular_file) == -1) {
    dprintf("Failed to allocate a new direntry for the inode.\n");
    goto close_parent_return_null;
  }
  // Allocate the memory for the file.
  // vfs_file_t *new_file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
  vfs_file_t *new_file = kmalloc(sizeof(vfs_file_t));
  if (new_file == NULL) {
    dprintf("Failed to allocate memory for the EXT2 file.\n");
    goto close_parent_return_null;
  }
  if (ext2_init_vfs_file(fs, new_file, &inode, inode_index, file_name,
                         strlen(file_name)) == -1) {
    dprintf("Failed to properly set the VFS file.\n");
    goto close_parent_return_null;
  }
  return new_file;
close_parent_return_null:
  vfs_close(parent);
  return NULL;
}

/// @brief Open the file at the given path and returns its file descriptor.
/// @param path  The path to the file.
/// @param flags The flags used to determine the behavior of the function.
/// @param mode  The mode with which we open the file.
/// @return The file descriptor of the opened file, otherwise returns -1.
static vfs_file_t *ext2_open(const char *path, int flags, mode_t mode) {
  dprintf("ext2_open(path: \"%s\", flags: %d, mode: %d)\n", path, flags, mode);
  // Get the absolute path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("Cannot get the absolute path for path `%s`.\n", path);
    return NULL;
  }
  // Get the EXT2 filesystem.
  ext2_filesystem_t *fs = get_ext2_filesystem(absolute_path);
  if (fs == NULL) {
    dprintf("Failed to get the EXT2 filesystem for absolute path `%s`.\n",
            absolute_path);
    return NULL;
  }
  // Prepare the structure for the direntry.
  ext2_dirent_t direntry;
  memset(&direntry, 0, sizeof(ext2_dirent_t));
  // Prepare the structure for the search.
  ext2_direntry_search_t search;
  memset(&search, 0, sizeof(ext2_direntry_search_t));
  // Initialize the search structure.
  search.direntry     = &direntry;
  search.block_index  = 0;
  search.block_offset = 0;
  search.parent_inode = 0;
  // First check, if a file with the given name already exists.
  if (!ext2_resolve_path(fs->root, absolute_path, &search)) {
    if (bitmask_check(flags, O_CREAT | O_EXCL)) {
      dprintf(
        "A file or directory already exists at `%s` (O_CREAT | O_EXCL).\n",
        absolute_path);
      return NULL;
    } else if (bitmask_check(flags, O_DIRECTORY) &&
               (direntry.file_type != ext2_file_type_directory)) {
      errno = ENOTDIR;
      return NULL;
    }
  } else {
    // If we need to create it, it's ok if it does not exist.
    if (bitmask_check(flags, O_CREAT)) {
      return ext2_creat(path, mode);
    } else {
      dprintf("The file does not exist `%s`.\n", absolute_path);
      errno = ENOENT;
      return NULL;
    }
  }
  // Prepare the structure for the inode.
  ext2_inode_t inode;
  memset(&inode, 0, sizeof(ext2_inode_t));
  // Get the inode associated with the directory entry.
  if (ext2_read_inode(fs, &inode, direntry.inode) == -1) {
    dprintf("Failed to read the inode of `%s`.\n", direntry.name);
    return NULL;
  }

  if (!ext2_valid_permissions(flags, inode.mode, inode.uid, inode.gid)) {
    errno = EACCES;
    return NULL;
  }

  vfs_file_t *file = ext2_find_vfs_file_with_inode(fs, direntry.inode);
  if (file == NULL) {
    // Allocate the memory for the file.
    // file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    file = kmalloc(sizeof(vfs_file_t));
    if (file == NULL) {
      dprintf("Failed to allocate memory for the EXT2 file.\n");
      return NULL;
    }
    if (ext2_init_vfs_file(fs, file, &inode, direntry.inode, direntry.name,
                           direntry.name_len) == -1) {
      dprintf("Failed to properly set the VFS file.\n");
      return NULL;
    }
    // Add the vfs_file to the list of associated files.
    list_head_insert_before(&file->siblings, &fs->opened_files);
  }
  return file;
}

static int ext2_unlink(const char *path) {
  dprintf("ext2_unlink(%s)\n", path);
  // Get the absolute path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("Cannot get the absolute path for path `%s`.\n", path);
    return -ENOENT;
  }
  // Get the name of the entry we want to unlink.
  char *name = basename(absolute_path);
  if (name == NULL) {
    dprintf("Cannot get the basename from the absolute path `%s`.\n",
            absolute_path);
    return -ENOENT;
  }
  // Get the EXT2 filesystem.
  ext2_filesystem_t *fs = get_ext2_filesystem(absolute_path);
  if (fs == NULL) {
    dprintf("Failed to get the EXT2 filesystem for absolute path `%s`.\n",
            absolute_path);
    return -ENOENT;
  }
  // Prepare the structure for the direntry.
  ext2_dirent_t direntry;
  memset(&direntry, 0, sizeof(ext2_dirent_t));
  // Prepare the structure for the search.
  ext2_direntry_search_t search;
  memset(&search, 0, sizeof(ext2_direntry_search_t));
  // Initialize the search structure.
  search.direntry     = &direntry;
  search.block_index  = 0;
  search.block_offset = 0;
  search.parent_inode = 0;
  // Resolve the path to the directory entry.
  if (ext2_resolve_path(fs->root, absolute_path, &search)) {
    dprintf("Failed to resolve path `%s`.\n", absolute_path);
    return -ENOENT;
  }
  // Get the inode associated with the parent directory entry.
  ext2_inode_t parent_inode;
  if (ext2_read_inode(fs, &parent_inode, search.parent_inode) == -1) {
    dprintf("ext2_stat(%s): Failed to read the inode of parent of `%s`.\n",
            path, direntry.name);
    return -ENOENT;
  }
  // Allocate the cache and clean it.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  memset(cache, 0, fs->block_size);
  // Read the block where the direntry resides.
  if (ext2_read_inode_block(fs, &parent_inode, search.block_index, cache) ==
      -1) {
    dprintf("Failed to read the parent inode block `%d`\n", search.block_index);
    goto free_cache_return_error;
  }
  // Get a pointer to the direntry.
  ext2_dirent_t *actual_dirent =
    (ext2_dirent_t *)((uintptr_t)cache + search.block_offset);
  if (actual_dirent == NULL) {
    dprintf("We found a NULL ext2_dirent_t\n");
    goto free_cache_return_error;
  }
  // Set the inode to zero.
  actual_dirent->inode = 0;
  // Write back the parent directory block.
  if (!ext2_write_inode_block(fs, &parent_inode, search.parent_inode,
                              search.block_index, cache)) {
    dprintf("Failed to write the inode block `%d`\n", search.block_index);
    goto free_cache_return_error;
  }
  // Read the inode of the direntry we want to unlink.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, direntry.inode) == -1) {
    dprintf("Failed to read the inode of `%s`.\n", direntry.name);
    goto free_cache_return_error;
  }
  if (inode.links_count > 0) {
    inode.links_count--;
    // Update the inode.
    if (ext2_write_inode(fs, &inode, direntry.inode) == -1) {
      dprintf("Failed to update the inode of `%s`.\n", direntry.name);
      goto free_cache_return_error;
    }
  }
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return 0;
free_cache_return_error:
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return -1;
}

/// @brief Closes the given file.
/// @param file The file structure.
static int ext2_close(vfs_file_t *file) {
  // Get the filesystem.
  ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
  if (fs == NULL) {
    dprintf("The file does not belong to an EXT2 filesystem `%s`.\n",
            file->name);
    return -1;
  }
  // We cannot close the root.
  if (file == fs->root) {
    return -1;
  }
  dprintf("ext2_close(ino: %d, file: \"%s\")\n", file->ino, file->name);
  // Remove the file from the list of opened files.
  list_head_remove(&file->siblings);
  // Free the cache.
  // kmem_cache_free(file);
  kfree(file);
  return 0;
}

/// @brief Reads from the file identified by the file descriptor.
/// @param file The file.
/// @param buffer Buffer where the read content must be placed.
/// @param offset Offset from which we start reading from the file.
/// @param nbyte The number of bytes to read.
/// @return The number of red bytes.
static ssize_t ext2_read(vfs_file_t *file, char *buffer, off_t offset,
                         size_t nbyte) {
  //dprintf("ext2_read(%s, %p, %d, %d)\n", file->name, buffer, offset, nbyte);
  // Get the filesystem.
  ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
  if (fs == NULL) {
    dprintf("The file does not belong to an EXT2 filesystem `%s`.\n",
            file->name);
    return -1;
  }
  // Get the inode associated with the file.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, file->ino) == -1) {
    dprintf("Failed to read the inode `%s`.\n", file->name);
    return -1;
  }
  return ext2_read_inode_data(fs, &inode, file->ino, offset, nbyte, buffer);
}

/// @brief Writes the given content inside the file.
/// @param file The file descriptor of the file.
/// @param buffer The content to write.
/// @param offset Offset from which we start writing in the file.
/// @param nbyte The number of bytes to write.
/// @return The number of written bytes.
static ssize_t ext2_write(vfs_file_t *file, const void *buffer, off_t offset,
                          size_t nbyte) {
  // Get the filesystem.
  ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
  if (fs == NULL) {
    dprintf("The file does not belong to an EXT2 filesystem `%s`.\n",
            file->name);
    return -1;
  }
  // Get the inode associated with the file.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, file->ino) == -1) {
    dprintf("Failed to read the inode `%s`.\n", file->name);
    return -1;
  }
  return ext2_write_inode_data(fs, &inode, file->ino, offset, nbyte,
                               (char *)buffer);
}

/// @brief Repositions the file offset inside a file.
/// @param file the file we are working with.
/// @param offset the offest to use for the operation.
/// @param whence the type of operation.
/// @return  Upon successful completion, returns the resulting offset
/// location as measured in bytes from the beginning of the file. On
/// error, the value (off_t) -1 is returned and errno is set to
/// indicate the error.
static off_t ext2_lseek(vfs_file_t *file, off_t offset, int whence) {
  // Get the filesystem.
  ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
  if (fs == NULL) {
    dprintf("The file does not belong to an EXT2 filesystem `%s`.\n",
            file->name);
    return -EPERM;
  }
  // Get the inode associated with the file.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, file->ino) == -1) {
    dprintf("Failed to read the inode `%s`.\n", file->name);
    return -ENOENT;
  }
  // Deal with the specific whence.
  switch (whence) {
    case SEEK_END:
      offset += inode.size;
      break;
    case SEEK_CUR:
      if (offset == 0) {
        return file->f_pos;
      }
      offset += file->f_pos;
      break;
    case SEEK_SET:
      break;
    default:
      return -EINVAL;
  }
  if (offset >= 0) {
    if (offset != file->f_pos) {
      file->f_pos = offset;
    }
    return offset;
  }
  return -EINVAL;
}

/// @brief Saves the information concerning the file.
/// @param inode The inode containing the data.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int __ext2_stat(ext2_inode_t *inode, stat_t *stat) {
  stat->st_mode  = inode->mode;
  stat->st_uid   = inode->uid;
  stat->st_gid   = inode->gid;
  stat->st_size  = inode->size;
  stat->st_atime = inode->atime;
  stat->st_mtime = inode->mtime;
  stat->st_ctime = inode->ctime;
  return 0;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param file The file struct.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int ext2_fstat(vfs_file_t *file, stat_t *stat) {
  // Get the filesystem.
  ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
  if (fs == NULL) {
    dprintf("The file does not belong to an EXT2 filesystem `%s`.\n",
            file->name);
    return -EPERM;
  }
  // Get the inode associated with the file.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, file->ino) == -1) {
    dprintf("Failed to read the inode `%s`.\n", file->name);
    return -ENOENT;
  }
  /// ID of device containing file.
  stat->st_dev = fs->block_device->ino;
  // Set the inode.
  stat->st_ino = file->ino;
  // Set the rest of the structure.
  return __ext2_stat(&inode, stat);
}

static int ext2_ioctl(vfs_file_t *file, int request, void *data) {
  return -1;
}

/// @brief Reads contents of the directories to a dirent buffer, updating
///        the offset and returning the number of written bytes in the buffer,
///        it assumes that all paths are well-formed.
/// @param file  The directory handler.
/// @param dirp  The buffer where the data should be written.
/// @param doff  The offset inside the buffer where the data should be written.
/// @param count The maximum length of the buffer.
/// @return The number of written bytes in the buffer.
static int ext2_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff,
                         size_t count) {
  dprintf("ext2_getdents(%s, %p, %d, %d)\n", file->name, dirp, doff, count);
  // Get the filesystem.
  ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
  if (fs == NULL) {
    dprintf("The file does not belong to an EXT2 filesystem `%s`.\n",
            file->name);
    return -ENOENT;
  }
  // Get the inode associated with the file.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, file->ino) == -1) {
    dprintf("Failed to read the inode (%d).\n", file->ino);
    return -ENOENT;
  }
  uint32_t current = 0, written = 0;
  // Allocate the cache.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  // Clean the cache.
  memset(cache, 0, fs->block_size);
  // Initialize the iterator.
  ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, &inode);
  for (; ext2_direntry_iterator_valid(&it) && (written < count);
       ext2_direntry_iterator_next(&it)) {
    // Skip unused inode.
    if (it.direntry->inode == 0)
      continue;
    // Skip if already provided.
    current += sizeof(dirent_t);
    if (current <= doff)
      continue;
    // Write on current directory entry data.
    dirp->d_ino  = it.direntry->inode;
    dirp->d_type = ext2_file_type_to_vfs_file_type(it.direntry->file_type);
    memset(dirp->d_name, 0, NAME_MAX);
    strncpy(dirp->d_name, it.direntry->name, it.direntry->name_len);
    dirp->d_off    = it.direntry->rec_len;
    dirp->d_reclen = it.direntry->rec_len;
    // Increment the amount written.
    written += sizeof(dirent_t);
    // Move to next writing position.
    ++dirp;
  }
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return written;
}

static int ext2_mkdir(const char *path, mode_t permission) {
  dprintf("\next2_mkdir(%s, %d)\n", path, permission);
  // Get the absolute path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("Cannot get the absolute path for path `%s`.\n", path);
    return -ENOENT;
  }
  // Get the EXT2 filesystem.
  ext2_filesystem_t *fs = get_ext2_filesystem(absolute_path);
  if (fs == NULL) {
    dprintf("Failed to get the EXT2 filesystem for absolute path `%s`.\n",
            absolute_path);
    return -ENOENT;
  }
  // Prepare the structure for the direntry.
  ext2_dirent_t direntry;
  memset(&direntry, 0, sizeof(ext2_dirent_t));
  // Prepare an inode, it will come in handy either way.
  ext2_inode_t inode;
  // Search if the entry already exists.
  if (!ext2_resolve_path_direntry(fs->root, absolute_path, &direntry)) {
    dprintf("Directory already exists.\n");
    return -EEXIST;
  }
  // Get the parent directory.
  char *parent_path = dirname(path);
  if (strcmp(parent_path, path) == 0) {
    dprintf("Failed to properly get the parent directory (%s == %s).\n",
            parent_path, path);
    return -ENOENT;
  }
  // Get the parent VFS node.
  vfs_file_t *parent = vfs_open(parent_path, O_RDONLY, 0);
  if (parent == NULL) {
    dprintf("Failed to open parent directory (%s).\n", parent_path);
    return -ENOENT;
  }
  // Set the inode mode.
  uint32_t mode = EXT2_S_IFDIR;
  mode |= 0xFFF & permission;
  // Get the group index of the parent.
  uint32_t group_index = ext2_get_group_index_from_inode(fs, parent->ino);
  // Create and initialize the new inode.
  int inode_index = ext2_create_inode(fs, &inode, mode, group_index);
  if (inode_index == -1) {
    dprintf("Failed to create a new inode inside `%s` (group index: %d).\n",
            parent->name, group_index);
    // Close the parent directory.
    vfs_close(parent);
    return -ENOENT;
  }
  // Increase the number of directories inside the group.
  fs->block_groups[group_index].used_dirs_count += 1;
  // Update the bgdt.
  ext2_write_bgdt(fs);
  // Write the inode.
  if (ext2_write_inode(fs, &inode, inode_index) == -1) {
    dprintf("Failed to write the newly created inode.\n");
    // Close the parent directory.
    vfs_close(parent);
    return -ENOENT;
  }
  // Create a directory entry for the directory.
  if (ext2_allocate_direntry(fs, parent->ino, inode_index, basename(path),
                             ext2_file_type_directory) == -1) {
    dprintf("Failed to allocate a new direntry for the inode.\n");
    // Close the parent directory.
    vfs_close(parent);
    return -ENOENT;
  }
  // Create a directory entry, inside the new directory, pointing to itself.
  if (ext2_allocate_direntry(fs, inode_index, inode_index, ".",
                             ext2_file_type_directory) == -1) {
    dprintf("Failed to allocate a new direntry for the inode.\n");
    // Close the parent directory.
    vfs_close(parent);
    return -ENOENT;
  }
  // Create a directory entry, inside the new directory, pointing to its parent.
  if (ext2_allocate_direntry(fs, inode_index, parent->ino, "..",
                             ext2_file_type_directory) == -1) {
    dprintf("Failed to allocate a new direntry for the inode.\n");
    // Close the parent directory.
    vfs_close(parent);
    return -ENOENT;
  }
  // Close the parent directory.
  vfs_close(parent);
  return 0;
}

static int ext2_rmdir(const char *path) {
  dprintf("ext2_unlink(%s)\n", path);
  // Get the absolute path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("Cannot get the absolute path for path `%s`.\n", path);
    return -ENOENT;
  }
  // Get the name of the entry we want to unlink.
  char *name = basename(absolute_path);
  if (name == NULL) {
    dprintf("Cannot get the basename from the absolute path `%s`.\n",
            absolute_path);
    return -ENOENT;
  }
  // Get the EXT2 filesystem.
  ext2_filesystem_t *fs = get_ext2_filesystem(absolute_path);
  if (fs == NULL) {
    dprintf("Failed to get the EXT2 filesystem for absolute path `%s`.\n",
            absolute_path);
    return -ENOENT;
  }
  // Prepare the structure for the direntry.
  ext2_dirent_t direntry;
  memset(&direntry, 0, sizeof(ext2_dirent_t));
  // Prepare the structure for the search.
  ext2_direntry_search_t search;
  memset(&search, 0, sizeof(ext2_direntry_search_t));
  // Initialize the search structure.
  search.direntry     = &direntry;
  search.block_index  = 0;
  search.block_offset = 0;
  search.parent_inode = 0;
  // Resolve the path to the directory entry.
  if (ext2_resolve_path(fs->root, absolute_path, &search)) {
    dprintf("Failed to resolve path `%s`.\n", absolute_path);
    return -ENOENT;
  }
  // Get the inode associated with the parent directory entry.
  ext2_inode_t parent_inode;
  if (ext2_read_inode(fs, &parent_inode, search.parent_inode) == -1) {
    dprintf("ext2_stat(%s): Failed to read the inode of parent of `%s`.\n",
            path, direntry.name);
    return -ENOENT;
  }

  // Allocate the cache and clean it.
  // uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
  uint8_t *cache = kmalloc(sizeof(uint8_t) * fs->block_size);
  memset(cache, 0, fs->block_size);

  // Read the inode of the direntry we want to unlink.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, direntry.inode) == -1) {
    dprintf("Failed to read the inode of `%s`.\n", direntry.name);
    goto free_cache_return_error;
  }
  // Check if the directory is empty, if it enters the loop then it means it is not empty.
  if (!ext2_directory_is_empty(fs, cache, &inode)) {
    dprintf("The directory is not empty `%s`.\n", direntry.name);
    // kmem_cache_free(cache);
    kfree(cache);
    return -ENOTEMPTY;
  }
  // Reduce the number of links to the inode.
  if (inode.links_count > 0) {
    inode.links_count--;
    // Update the inode.
    if (ext2_write_inode(fs, &inode, direntry.inode) == -1) {
      dprintf("Failed to update the inode of `%s`.\n", direntry.name);
      goto free_cache_return_error;
    }
  }

  // Read the block where the direntry resides.
  if (ext2_read_inode_block(fs, &parent_inode, search.block_index, cache) ==
      -1) {
    dprintf("Failed to read the parent inode block `%d`\n", search.block_index);
    goto free_cache_return_error;
  }
  // Get a pointer to the direntry.
  ext2_dirent_t *actual_dirent =
    (ext2_dirent_t *)((uintptr_t)cache + search.block_offset);
  if (actual_dirent == NULL) {
    dprintf("We found a NULL ext2_dirent_t\n");
    goto free_cache_return_error;
  }
  // Set the inode to zero.
  actual_dirent->inode = 0;
  // Write back the parent directory block.
  if (!ext2_write_inode_block(fs, &parent_inode, search.parent_inode,
                              search.block_index, cache)) {
    dprintf("Failed to write the inode block `%d`\n", search.block_index);
    goto free_cache_return_error;
  }

  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return 0;
free_cache_return_error:
  // Free the cache.
  // kmem_cache_free(cache);
  kfree(cache);
  return -1;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param path The path where the file resides.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int ext2_stat(const char *path, stat_t *stat) {
  dprintf("ext2_stat(%s, %p)\n", path, stat);
  // Get the absolute path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("Cannot get the absolute path for path `%s`.\n", path);
    return -ENOENT;
  }
  // Get the EXT2 filesystem.
  ext2_filesystem_t *fs = get_ext2_filesystem(absolute_path);
  if (fs == NULL) {
    dprintf("Failed to get the EXT2 filesystem for absolute path `%s`.\n",
            absolute_path);
    return -ENOENT;
  }
  // Prepare the structure for the direntry.
  ext2_dirent_t direntry;
  memset(&direntry, 0, sizeof(ext2_dirent_t));
  // Resolve the path.
  if (ext2_resolve_path_direntry(fs->root, absolute_path, &direntry)) {
    dprintf("Failed to resolve path `%s`.\n", absolute_path);
    return -ENOENT;
  }
  // Get the inode associated with the directory entry.
  ext2_inode_t inode;
  if (ext2_read_inode(fs, &inode, direntry.inode) == -1) {
    dprintf("ext2_stat(%s): Failed to read the inode of `%s`.\n", path,
            direntry.name);
    return -ENOENT;
  }
  /// ID of device containing file.
  stat->st_dev = fs->block_device->ino;
  // Set the inode.
  stat->st_ino = direntry.inode;
  // Set the rest of the structure.
  return __ext2_stat(&inode, stat);
}

/// @brief Mounts the block device as an EXT2 filesystem.
/// @param block_device the block device formatted as EXT2.
/// @return the VFS root node of the EXT2 filesystem.
static vfs_file_t *ext2_mount(vfs_file_t *block_device, const char *path) {
  // Create the ext2 filesystem.
  ext2_filesystem_t *fs = kmalloc(sizeof(ext2_filesystem_t));
  // Clean the memory.
  memset(fs, 0, sizeof(ext2_filesystem_t));
  // Initialize the filesystem spinlock.
  spinlock_init(&fs->spinlock);
  // Initialize the list of opened files.
  list_head_init(&fs->opened_files);
  // Set the pointer to the block device.
  fs->block_device = block_device;
  // Read the superblock.
  if (ext2_read_superblock(fs) == -1) {
    dprintf("Failed to read the superblock table at 1024.\n");
    // Free just the filesystem.
    goto free_filesystem;
  }
  // Check the superblock magic number.
  if (fs->superblock.magic != EXT2_SUPERBLOCK_MAGIC) {
    dprintf("Wrong magic number, it is not an EXT2 filesystem.\n");
    ext2_dump_superblock(&fs->superblock);
    // Free just the filesystem.
    goto free_filesystem;
  }
  // Compute the volume size.
  fs->block_size = 1024U << fs->superblock.log_block_size;
  // Initialize the buffer cache.
  // TODO: Slab allocator
  // fs->ext2_buffer_cache = kmem_cache_create("ext2_buffer_cache", fs->block_size,
  //                                           fs->block_size, GFP_KERNEL, NULL,
  //                                           NULL);
  // Compute the maximum number of inodes per block.
  fs->inodes_per_block_count = fs->block_size / fs->superblock.inode_size;
  // Compute the number of blocks per block. This value is mostly used for
  // inodes.
  // If you check inside the inode structure you will find the `blocks_count`
  // field. A 32-bit value representing the total number of 512-bytes blocks
  // reserved to contain the data of this inode, regardless if these blocks
  // are used or not. The block numbers of these reserved blocks are contained
  // in the `block` array.
  // Since this value represents 512-byte blocks and not file system blocks,
  // this value should not be directly used as an index to the `block` array.
  // Rather, the maximum index of the `block` array should be computed from
  //      inode->blocks_count / ((1024 << superblock->log_block_size) / 512)
  // or once simplified:
  //      inode->blocks_count / (2 << superblock->log_block_size)
  // Now we just need to precompute the right part.
  fs->blocks_per_block_count = fs->block_size / 512U;
  // Compute the number of block pointers per block.
  fs->pointers_per_block = fs->block_size / 4U;
  // Compute the index of indirect blocks.
  fs->indirect_blocks_index = EXT2_INDIRECT_BLOCKS + fs->pointers_per_block;
  fs->doubly_indirect_blocks_index =
    EXT2_INDIRECT_BLOCKS +
    fs->pointers_per_block * (fs->pointers_per_block + 1);
  fs->trebly_indirect_blocks_index =
    fs->doubly_indirect_blocks_index +
    (fs->pointers_per_block * fs->pointers_per_block) *
      (fs->pointers_per_block + 1);
  // Compute the number of block groups.
  fs->block_groups_count =
    fs->superblock.blocks_count / fs->superblock.blocks_per_group;
  if (fs->superblock.blocks_per_group * fs->block_groups_count <
      fs->superblock.blocks_count) {
    fs->block_groups_count += 1;
  }
  // The block group descriptor table starts on the first block following the
  // superblock. This would be the second block for 2KiB and larger block file systems.
  if (fs->block_size > KB) {
    fs->bgdt_start_block = 1;
  } else {
    // However, it would be the third block on a 1KiB block file system.
    fs->bgdt_start_block = 2;
  }
  // The block group descriptor table ends a certain amount of blocks.
  fs->bgdt_end_block =
    fs->bgdt_start_block +
    ((sizeof(ext2_group_descriptor_t) * fs->block_groups_count) /
     fs->block_size) +
    1;
  // Compute the length in blocks of the BGDT.
  fs->bgdt_length = fs->bgdt_end_block - fs->bgdt_start_block;

  // Now, we have the size of a block, calculate the location of the Block
  // Group Descriptor Table (BGDT). The BGDT is located directly after the
  // superblock, so obtain the block of the superblock first.
  fs->block_groups = kmalloc(fs->block_size * fs->bgdt_length);
  if (fs->block_groups == NULL) {
    dprintf("Failed to allocate memory for the block buffer.\n");
    // Free just the filesystem.
    goto free_filesystem;
  }

  // Try to read the BGDT.
  if (ext2_read_bgdt(fs) == -1) {
    dprintf("Failed to read the BGDT.\n");
    // Free the block_groups and the filesystem.
    goto free_block_groups;
  }

  // We need the root inode in order to set the root file.
  ext2_inode_t root_inode;
  if (ext2_read_inode(fs, &root_inode, 2U) == -1) {
    dprintf("Failed to set the root inode.\n");
    // Free the block_buffer, the block_groups and the filesystem.
    goto free_block_buffer;
  }
  if ((root_inode.mode & EXT2_S_IFDIR) != EXT2_S_IFDIR) {
    dprintf("The root is not a directory.\n");
    // Free the block_buffer, the block_groups and the filesystem.
    goto free_block_buffer;
  }
  // Allocate the memory for the root.
  // fs->root = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
  fs->root = kmalloc(sizeof(vfs_file_t));
  if (!fs->root) {
    dprintf("Failed to allocate memory for the EXT2 root file!\n");
    // Free the block_buffer, the block_groups and the filesystem.
    goto free_block_buffer;
  }
  if (ext2_init_vfs_file(fs, fs->root, &root_inode, 2, path, strlen(path)) ==
      -1) {
    dprintf("Failed to set the EXT2 root.\n");
    // Free the block_buffer, the block_groups and the filesystem.
    goto free_all;
  }
  // Add the root to the list of opened files.
  list_head_insert_before(&fs->root->siblings, &fs->opened_files);

  // Dump the filesystem details for debugging.
  ext2_dump_filesystem(fs);
  // Dump the superblock details for debugging.
  ext2_dump_superblock(&fs->superblock);
  // Dump the block group descriptor table.
  ext2_dump_bgdt(fs);

  return fs->root;

free_all:
  // Free the memory occupied by the root.
  // kmem_cache_free(fs->root);
  kfree(fs->root);
free_block_buffer:
  // Free the memory occupied by the block buffer.
  // kmem_cache_destroy(fs->ext2_buffer_cache);
free_block_groups:
  // Free the memory occupied by the block groups.
  kfree(fs->block_groups);
free_filesystem:
  // Free the memory occupied by the filesystem.
  kfree(fs);
  return NULL;
}

// ============================================================================
// Initialization Functions
// ============================================================================

static vfs_file_t *ext2_mount_callback(const char *path, const char *device) {
  // Allocate a variable for the path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(device, absolute_path)) {
    dprintf("ext2_mount_callback(%s, %s): Cannot get the absolute path.", path,
            device);
    return NULL;
  }
  super_block_t *sb = vfs_get_superblock(absolute_path);
  if (sb == NULL) {
    dprintf(
      "ext2_mount_callback(%s, %s): Cannot find the superblock at absolute path `%s`!\n",
      path, device, absolute_path);
    return NULL;
  }
  vfs_file_t *block_device = sb->root;
  if (block_device == NULL) {
    dprintf("ext2_mount_callback(%s, %s): Cannot find the superblock root.",
            path, device);
    return NULL;
  }
  if (block_device->flags != DT_BLK) {
    dprintf("ext2_mount_callback(%s, %s): The device is not a block device.\n",
            path, device);
    return NULL;
  }
  return ext2_mount(block_device, path);
}

/// Filesystem information.
static file_system_type ext2_file_system_type = {
  .name     = EXT2,
  .fs_flags = 0,
  .mount    = ext2_mount_callback,
};

int ext2_init(void) {
  // Register the filesystem.
  vfs_register_filesystem(&ext2_file_system_type);
  return 0;
}

int ext2_finalize(void) {
  vfs_unregister_filesystem(&ext2_file_system_type);
  return 0;
}
