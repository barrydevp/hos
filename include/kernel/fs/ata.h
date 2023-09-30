#pragma once

#include <kernel/types.h>
#include <kernel/fs/vfs.h>

#define ATA_SECTOR_SIZE 512 ///< The sector size.
#define ATA_DMA_SIZE    512 ///< The size of the DMA area.
///
/// @brief ATA Error Bits
typedef enum {
  ata_err_amnf  = 0, ///< Address mark not found.
  ata_err_tkznf = 1, ///< Track zero not found.
  ata_err_abrt  = 2, ///< Aborted command.
  ata_err_mcr   = 3, ///< Media change request.
  ata_err_idnf  = 4, ///< ID not found.
  ata_err_mc    = 5, ///< Media changed.
  ata_err_unc   = 6, ///< Uncorrectable data error.
  ata_err_bbk   = 7, ///< Bad Block detected.
} ata_error_t;

/// @brief ATA Status Bits
typedef enum {
  ata_status_err  = 0, ///< Indicates an error occurred.
  ata_status_idx  = 1, ///< Index. Always set to zero.
  ata_status_corr = 2, ///< Corrected data. Always set to zero.
  ata_status_drq =
    3, ///< Set when the drive has PIO data to transfer, or is ready to accept PIO data.
  ata_status_srv = 4, ///< Overlapped Mode Service Request.
  ata_status_df  = 5, ///< Drive Fault Error (does not set ERR).
  ata_status_rdy =
    6, ///< Bit is clear when drive is spun down, or after an error. Set otherwise.
  ata_status_bsy =
    7, ///< The drive is preparing to send/receive data (wait for it to clear).
} ata_status_t;

/// @brief ATA Control Bits
typedef enum {
  ata_control_zero = 0x00, ///< Always set to zero.
  ata_control_nien =
    0x02, ///< Set this to stop the current device from sending interrupts.
  ata_control_srst =
    0x04, ///< Set, then clear (after 5us), this to do a "Software Reset" on all ATA drives on a bus, if one is misbehaving.
  ata_control_hob =
    0x80, ///< Set this to read back the High Order Byte (HOB) of the last LBA48 value sent to an IO port.
} ata_control_t;

/// @brief Types of ATA devices.
typedef enum {
  ata_dev_type_unknown,   ///< Device type not recognized.
  ata_dev_type_no_device, ///< No device detected.
  ata_dev_type_pata,      ///< Parallel ATA drive.
  ata_dev_type_sata,      ///< Serial ATA drive.
  ata_dev_type_patapi,    ///< Parallel ATAPI drive.
  ata_dev_type_satapi     ///< Serial ATAPI drive.
} ata_device_type_t;

/// @brief Values used to manage bus mastering.
typedef enum {
  ata_bm_stop_bus_master =
    0x00, ///< Halts bus master operations of the controller.
  ata_bm_start_bus_master =
    0x01, ///< Enables bus master operation of the controller.
} ata_bus_mastering_command_t;

/// @brief DMA specific commands.
typedef enum {
  ata_dma_command_read = 0xC8, ///< Read DMA with retries (28 bit LBA).
  ata_dma_command_read_no_retry =
    0xC9,                       ///< Read DMA without retries (28 bit LBA).
  ata_dma_command_write = 0xCA, ///< Write DMA with retries (28 bit LBA).
  ata_dma_command_write_no_retry =
    0xCB, ///< Write DMA without retries (28 bit LBA).
} ata_dma_command_t;

/// @brief ATA identity commands.
typedef enum {
  ata_command_pata_ident   = 0xEC, ///< Identify Device.
  ata_command_patapi_ident = 0xA1, ///< Identify Device.
} ata_identity_command_t;

/// @brief IDENTIFY device data (response to 0xEC).
typedef struct ata_identify_t {
  /// Word      0 : General configuration.
  struct {
    /// Reserved.
    uint16_t reserved1 : 1;
    /// This member is no longer used.
    uint16_t retired3 : 1;
    /// Indicates that the response was incomplete.
    uint16_t response_incomplete : 1;
    /// This member is no longer used.
    uint16_t retired2 : 3;
    /// Indicates when set to 1 that the device is fixed.
    uint16_t fixed_fevice : 1;
    /// Indicates when set to 1 that the media is removable.
    uint16_t removable_media : 1;
    /// This member is no longer used.
    uint16_t retired1 : 7;
    /// Indicates when set to 1 that the device is an ATA device.
    uint16_t device_type : 1;
  } general_configuration;
  /// Word   1-9  : Unused.
  uint16_t unused1[9];
  /// Word  10-19 : Contains the serial number of the device.
  uint8_t serial_number[20];
  /// Word  20-22 : Unused.
  uint16_t unused2[3];
  /// Word  23-26 : Contains the revision number of the device's firmware.
  uint8_t firmware_revision[8];
  /// Word  27-46 : Contains the device's model number.
  uint8_t model_number[40];
  /// Word     47 : Maximum number of sectors that shall be transferred per interrupt.
  uint8_t maximum_block_transfer;
  /// Word     48 : Unused.
  uint8_t unused3;
  /// Word  49-50 :
  struct {
    /// Unused.
    uint8_t current_long_physical_sector_alignment : 2;
    /// Reserved.
    uint8_t reserved_byte49 : 6;
    /// Indicates that the device supports DMA operations.
    uint8_t dma_supported : 1;
    /// Indicates that the device supports logical block addressing.
    uint8_t lba_supported : 1;
    /// Indicates when set to 1 that I/O channel ready is disabled for the device.
    uint8_t io_rdy_disable : 1;
    /// Indicates when set to 1 that I/O channel ready is supported by the device.
    uint8_t io_rdy_supported : 1;
    /// Reserved.
    uint8_t reserved1 : 1;
    /// Indicates when set to 1 that the device supports standby timers.
    uint8_t stand_by_timer_support : 1;
    /// Reserved.
    uint8_t reserved2 : 2;
    /// Reserved.
    uint16_t reserved_word50;
  } capabilities;
  /// Word  51-52 : Obsolete.
  uint16_t unused4[2];
  /// Word     53 : Bit 0 = obsolete; Bit 1 = words 70:64 valid; bit 2 = word 88 valid.
  uint16_t valid_ext_data;
  /// Word  54-58 : Obsolete.
  uint16_t unused5[5];
  /// Word     59 : Indicates the multisector setting.
  uint8_t current_multisector_setting;
  /// Indicates when TRUE that the multisector setting is valid.
  uint8_t multisector_setting_valid : 1;
  /// Reserved.
  uint8_t reserved_byte59 : 3;
  /// The device supports the sanitize command.
  uint8_t sanitize_feature_supported : 1;
  /// The device supports cryptographic erase.
  uint8_t crypto_scramble_ext_command_supported : 1;
  /// The device supports block overwrite.
  uint8_t overwrite_ext_command_supported : 1;
  /// The device supports block erase.
  uint8_t block_erase_ext_command_supported : 1;
  /// Word  60-61 : Contains the total number of 28 bit LBA addressable sectors on the drive.
  uint32_t sectors_28;
  /// Word  62-99 : We do not care for these right now.
  uint16_t unused6[38];
  /// Word 100-103: Contains the total number of 48 bit addressable sectors on the drive.
  uint64_t sectors_48;
  /// Word 104-256: We do not care for these right now.
  uint16_t unused7[152];
} ata_identify_t;

/// @brief Physical Region Descriptor Table (PRDT) entry.
/// @details
/// The physical memory region to be transferred is described by a Physical
/// Region Descriptor (PRD). The data transfer will proceed until all regions
/// described by the PRDs in the table have been transferred.
/// Each Physical Region Descriptor entry is 8 bytes in length.
///         |    byte 3  |  byte 2  |  byte 1  |  byte 0    |
/// Dword 0 |  Memory Region Physical Base Address [31:1] |0|
/// Dword 1 |  EOT | reserved       | Byte Count [15:1]   |0|
typedef struct prdt_t {
  /// The first 4 bytes specify the byte address of a physical memory region.
  unsigned int physical_address;
  /// The next two bytes specify the count of the region in bytes (64K byte limit per region).
  unsigned short byte_count;
  /// Bit 7 of the last byte indicates the end of the table
  unsigned short end_of_table;
} prdt_t;

/// @brief Stores information about an ATA device.
typedef struct ata_device_t {
  /// Name of the device.
  char name[NAME_MAX];
  /// Name of the device.
  char path[PATH_MAX];
  /// Does the device support ATA Packet Interface (ATAPI).
  ata_device_type_t type;
  /// The "I/O" port base.
  unsigned io_base;
  /// I/O registers.
  struct {
    /// [R/W] Data Register. Read/Write PIO data bytes (16-bit).
    unsigned data;
    /// [R  ] Error Register. Read error generated by the last ATA command executed (8-bit).
    unsigned error;
    /// [  W] Features Register. Used to control command specific interface features (8-bit).
    unsigned feature;
    /// [R/W] Sector Count Register. Number of sectors to read/write (0 is a special value) (8-bit).
    unsigned sector_count;
    /// [R/W] Sector Number Register. This is CHS/LBA28/LBA48 specific (8-bit).
    unsigned lba_lo;
    /// [R/W] Cylinder Low Register. Partial Disk Sector address (8-bit).
    unsigned lba_mid;
    /// [R/W] Cylinder High Register. Partial Disk Sector address (8-bit).
    unsigned lba_hi;
    /// [R/W] Drive / Head Register. Used to select a drive and/or head. Supports extra address/flag bits (8-bit).
    unsigned hddevsel;
    /// [R  ] Status Register. Used to read the current status (8-bit).
    unsigned status;
    /// [  W] Command Register. Used to send ATA commands to the device (8-bit).
    unsigned command;
  } io_reg;
  /// The "Control" port base.
  unsigned control_base;
  /// If the device master or slave.
  unsigned slave;
  /// If the device is connected to the primary bus.
  bool_t primary;
  /// The device identity data.
  ata_identify_t identity;
  /// Bus Master Register. The "address" of the Bus Master Register is stored
  /// in BAR4, in the PCI Configuration Space of the disk controller. The Bus
  /// Master Register is generally a set of 16 sequential IO ports. It can
  /// also be a 16 byte memory mapped space.
  struct {
    /// The command byte has only 2 operational bits. All the rest should be
    /// 0. Bit 0 (value = 1) is the Start/Stop bit. Setting the bit puts the
    /// controller in DMA mode for that ATA channel, and it starts at the
    /// beginning of the respective PRDT. Clearing the bit terminates DMA
    /// mode for that ATA channel. If the controller was in the middle of a
    /// transfer, the remaining data is thrown away. Also, the controller
    /// does not remember how far it got in the PRDT. That information is
    /// lost, if the OS does not save it. The bit must be cleared when a
    /// transfer completes.
    /// Bit 3 (value = 8) is the Read/Write bit. This bit is a huge problem.
    /// The disk controller does not automatically detect whether the next
    /// disk operation is a read or write. You have to tell it, in advance,
    /// by setting this bit. Note that when reading from the disk, you must
    /// set this bit to 1, and clear it when writing to the disk. You must
    /// first stop DMA transfers (by clearing bit 0) before you can change
    /// the Read/Write bit! Please note all the bad consequences of clearing
    /// bit 0, above! The controller loses its place in the PRDT.
    /// In essence, this means that each PRDT must consist exclusively of
    /// either read or write entries. You set the Read/Write bit in advance,
    /// then "use up" the entire PRDT -- before you can do the opposite
    /// operation.
    unsigned command;
    /// The bits in the status byte are not usually useful. However, you are
    /// required to read it after every IRQ on disk reads anyway. Reading this
    /// byte may perform a necessary final cache flush of the DMA data to
    /// memory.
    unsigned status;
    /// Physical Region Descriptor Table (PRDT). The PRDT must be uint32_t
    /// aligned, contiguous in physical memory, and cannot cross a 64K boundary.
    unsigned prdt;
  } bmr;
  /// Pointer to the first entry of the PRDT.
  prdt_t *dma_prdt;
  /// Physical address of the first entry of the PRDT.
  uintptr_t dma_prdt_phys;
  /// Pointer to the DMA memory area.
  uint8_t *dma_start;
  /// Physical address of the DMA memory area.
  uintptr_t dma_start_phys;
  /// Device root file.
  vfs_file_t *fs_root;
} ata_device_t;

/// @brief Initializes the ATA drivers.
/// @return 0 on success, 1 on error.
int ata_init();

/// @brief De-initializes the ATA drivers.
/// @return 0 on success, 1 on error.
int ata_finalize();
