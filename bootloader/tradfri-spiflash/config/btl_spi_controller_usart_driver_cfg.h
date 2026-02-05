/**
 * @file btl_spi_controller_usart_driver_cfg.h
 * @brief SPI Peripheral Driver Configuration for TRÅDFRI Board
 *
 * CRITICAL CONFIGURATION for IKEA TRÅDFRI board SPI flash communication.
 * These pins connect EFR32MG1P132 to IS25LQ020B flash chip.
 *
 * Pin Mapping:
 *   PD13 (Pin 1)  - SPI CLK
 *   PD14 (Pin 2)  - SPI MISO (RX)
 *   PD15 (Pin 3)  - SPI MOSI (TX)
 *   PB11 (Pin 6)  - SPI CS
 *
 * USART Location: RXLOC21/TXLOC23/CLKLOC19 (for PD13/14/15 routing)
 */

#ifndef BTL_SPI_CONTROLLER_USART_DRIVER_CFG_H
#define BTL_SPI_CONTROLLER_USART_DRIVER_CFG_H

#include "em_device.h"
#include "em_gpio.h"

// USART peripheral selection
// TRÅDFRI uses USART1 for SPI flash communication
#define SL_USART_EXTFLASH_PERIPHERAL          USART1
#define SL_USART_EXTFLASH_PERIPHERAL_NO       1

// Clock source configuration
#define SL_USART_EXTFLASH_CLOCK               cmuClock_USART1

// ============================================================================
// PIN CONFIGURATION - TRÅDFRI SPECIFIC
// ============================================================================

// TX Pin (MOSI - Master Out, Slave In)
#define SL_USART_EXTFLASH_TX_PORT             gpioPortD
#define SL_USART_EXTFLASH_TX_PIN              15

// RX Pin (MISO - Master In, Slave Out)
#define SL_USART_EXTFLASH_RX_PORT             gpioPortD
#define SL_USART_EXTFLASH_RX_PIN              14

// CLK Pin (Clock)
#define SL_USART_EXTFLASH_CLK_PORT            gpioPortD
#define SL_USART_EXTFLASH_CLK_PIN             13

// CS Pin (Chip Select)
#define SL_USART_EXTFLASH_CS_PORT             gpioPortB
#define SL_USART_EXTFLASH_CS_PIN              11

// ============================================================================
// USART LOCATION CONFIGURATION - CRITICAL FOR EFR32MG1P
// ============================================================================

// For EFR32MG1 Series 1, USART routing uses location values
// PD13/PD14/PD15 pins map to USART1 with mixed LOC values:
//   TX=PD15 (LOC23), RX=PD14 (LOC21), CLK=PD13 (LOC19)

#if defined(_SILICON_LABS_32B_SERIES_1)
  // Series 1 uses ROUTELOC registers
  // Build route location value
  #define SL_USART_EXTFLASH_TX_LOC            _USART_ROUTELOC0_TXLOC_LOC23
  #define SL_USART_EXTFLASH_RX_LOC            _USART_ROUTELOC0_RXLOC_LOC21
  #define SL_USART_EXTFLASH_CLK_LOC           _USART_ROUTELOC0_CLKLOC_LOC19

  // Combined location register value
  #define SL_USART_EXTFLASH_ROUTELOC          (SL_USART_EXTFLASH_TX_LOC  | \
                                               SL_USART_EXTFLASH_RX_LOC  | \
                                               SL_USART_EXTFLASH_CLK_LOC)
#elif defined(_SILICON_LABS_32B_SERIES_2)
  // Series 2 uses different routing mechanism
  #error "Series 2 routing not implemented for TRÅDFRI"
#endif

// ============================================================================
// SPI TIMING CONFIGURATION
// ============================================================================

// Bitrate: 4 MHz (conservative for reliable operation)
// IS25LQ020B supports up to 104 MHz, but 4 MHz is sufficient and more reliable
#define SL_USART_EXTFLASH_BITRATE             4000000

// SPI Mode: Mode 0 (CPOL=0, CPHA=0)
// Clock polarity: idle low
// Clock phase: sample on leading edge
#define SL_USART_EXTFLASH_CLK_POLARITY        0  // CPOL = 0
#define SL_USART_EXTFLASH_CLK_PHASE           0  // CPHA = 0

// Master mode (EFR32 is SPI master, flash is slave)
#define SL_USART_EXTFLASH_MASTER              1

// MSB first (standard for SPI flash)
#define SL_USART_EXTFLASH_MSB_FIRST           1

// ============================================================================
// GPIO CONFIGURATION
// ============================================================================

// CS active level (active low for SPI flash)
#define SL_USART_EXTFLASH_CS_ACTIVE_LEVEL     0

// Initial CS state (high = inactive)
#define SL_USART_EXTFLASH_CS_INIT_STATE       1

// GPIO drive strength
#define SL_USART_EXTFLASH_GPIO_DRIVE_STRENGTH gpioDriveStrengthStrongAlternateStrong

// ============================================================================
// TIMING CONSTRAINTS
// ============================================================================

// CS setup time (time between CS assertion and first clock edge)
#define SL_USART_EXTFLASH_CS_SETUP_US         1

// CS hold time (time between last clock edge and CS de-assertion)
#define SL_USART_EXTFLASH_CS_HOLD_US          1

// Inter-transfer delay (between successive SPI transfers)
#define SL_USART_EXTFLASH_INTER_TRANSFER_US   0

// ============================================================================
// HELPER MACROS FOR BOOTLOADER CODE
// ============================================================================

// Initialize GPIO pins for SPI
#define BTL_SPI_INIT_PINS()                                       \
  do {                                                            \
    /* Configure MOSI (TX) */                                     \
    GPIO_PinModeSet(SL_USART_EXTFLASH_TX_PORT,                   \
                    SL_USART_EXTFLASH_TX_PIN,                     \
                    gpioModePushPull, 0);                         \
    /* Configure MISO (RX) */                                     \
    GPIO_PinModeSet(SL_USART_EXTFLASH_RX_PORT,                   \
                    SL_USART_EXTFLASH_RX_PIN,                     \
                    gpioModeInput, 0);                            \
    /* Configure CLK */                                           \
    GPIO_PinModeSet(SL_USART_EXTFLASH_CLK_PORT,                  \
                    SL_USART_EXTFLASH_CLK_PIN,                    \
                    gpioModePushPull, 0);                         \
    /* Configure CS (initially high = inactive) */               \
    GPIO_PinModeSet(SL_USART_EXTFLASH_CS_PORT,                   \
                    SL_USART_EXTFLASH_CS_PIN,                     \
                    gpioModePushPull, 1);                         \
  } while (0)

// CS control macros
#define BTL_SPI_CS_ASSERT()   GPIO_PinOutClear(SL_USART_EXTFLASH_CS_PORT, SL_USART_EXTFLASH_CS_PIN)
#define BTL_SPI_CS_DEASSERT() GPIO_PinOutSet(SL_USART_EXTFLASH_CS_PORT, SL_USART_EXTFLASH_CS_PIN)

// ============================================================================
// VERIFICATION
// ============================================================================

// Compile-time verification that USART1 is selected
#if (SL_USART_EXTFLASH_PERIPHERAL_NO != 1)
  #error "TRÅDFRI bootloader requires USART1 for SPI flash"
#endif

// Verify pin assignments match TRÅDFRI hardware
#if (SL_USART_EXTFLASH_TX_PIN != 15)
  #error "TRÅDFRI MOSI must be on PD15"
#endif
#if (SL_USART_EXTFLASH_RX_PIN != 14)
  #error "TRÅDFRI MISO must be on PD14"
#endif
#if (SL_USART_EXTFLASH_CLK_PIN != 13)
  #error "TRÅDFRI CLK must be on PD13"
#endif
#if (SL_USART_EXTFLASH_CS_PIN != 11)
  #error "TRÅDFRI CS must be on PB11"
#endif

#endif // BTL_SPI_CONTROLLER_USART_DRIVER_CFG_H
