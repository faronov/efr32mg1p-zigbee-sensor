/**
 * @file btl_spiflash_storage_cfg.h
 * @brief SPI Flash Storage Configuration for TRÅDFRI Board
 *
 * Configuration for ISSI IS25LQ020B (256KB / 2Mbit) SPI flash
 * used on IKEA TRÅDFRI board for OTA image storage.
 */

#ifndef BTL_SPIFLASH_STORAGE_CFG_H
#define BTL_SPIFLASH_STORAGE_CFG_H

// Flash Device Configuration
// IS25LQ020B specifications
#define BTL_STORAGE_SPIFLASH_DEVICE_SIZE      262144  // 256 KB (2 Mbit)
#define BTL_STORAGE_SPIFLASH_PAGE_SIZE        256     // 256 bytes per page
#define BTL_STORAGE_SPIFLASH_SECTOR_SIZE      4096    // 4 KB sectors

// JEDEC ID for IS25LQ020B
// This is used to verify the correct flash chip is present
#define BTL_STORAGE_SPIFLASH_JEDEC_ID         0x9D4012

// Flash command set (standard JEDEC commands)
#define BTL_STORAGE_SPIFLASH_SUPPORTS_QUAD_IO  0  // Quad mode not used

// Erase capabilities
#define BTL_STORAGE_SPIFLASH_SUPPORTS_4K_ERASE  1  // 4KB sector erase
#define BTL_STORAGE_SPIFLASH_SUPPORTS_32K_ERASE 1  // 32KB block erase
#define BTL_STORAGE_SPIFLASH_SUPPORTS_64K_ERASE 1  // 64KB block erase

// Timing specifications (in milliseconds)
#define BTL_STORAGE_SPIFLASH_PAGE_PROGRAM_MS    3    // Page program time
#define BTL_STORAGE_SPIFLASH_SECTOR_ERASE_MS    400  // 4KB sector erase time
#define BTL_STORAGE_SPIFLASH_BLOCK_ERASE_32K_MS 1600 // 32KB block erase time
#define BTL_STORAGE_SPIFLASH_BLOCK_ERASE_64K_MS 2000 // 64KB block erase time

// Storage Slot Configuration
// Slot 0: Used for OTA image download storage
#define BTL_STORAGE_SLOT0_START               0x0       // Start of flash
#define BTL_STORAGE_SLOT0_SIZE                262144    // Full 256KB

// Calculate end address
#define BTL_STORAGE_SLOT0_END                 (BTL_STORAGE_SLOT0_START + BTL_STORAGE_SLOT0_SIZE - 1)

// Enable storage verification
#define BTL_STORAGE_VERIFICATION_ENABLED      1

// SPI bitrate configuration
// IS25LQ020B supports up to 104 MHz, but we use conservative 4 MHz for reliability
#define BTL_STORAGE_SPIFLASH_FREQUENCY        4000000  // 4 MHz

// Power-down mode support (for battery-powered operation)
#define BTL_STORAGE_SPIFLASH_SUPPORTS_POWER_DOWN  1

// Write protection (not used on TRÅDFRI)
#define BTL_STORAGE_SPIFLASH_WP_ENABLED       0

// Address mode (3-byte addressing for 256KB flash)
#define BTL_STORAGE_SPIFLASH_USE_4BYTE_ADDR   0

#endif // BTL_SPIFLASH_STORAGE_CFG_H
