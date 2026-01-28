/**
 * @file bme280_board_config.h
 * @brief BME280 sensor I2C pin configuration for EFR32MG1P
 *
 * This file defines the I2C pins and instance used to communicate with the
 * BME280 sensor. Adjust these definitions to match your hardware setup.
 *
 * Default configuration for BRD4151A (EFR32MG1P232F256GM48):
 * - I2C0 instance
 * - SDA: PC10 (Expansion Header Pin 15)
 * - SCL: PC11 (Expansion Header Pin 16)
 *
 * HARDWARE NOTES:
 * - BME280 requires 3.3V power supply
 * - Add 4.7k pull-up resistors on SDA and SCL lines
 * - BME280 I2C address: 0x76 (SDO to GND) or 0x77 (SDO to VDDIO)
 */

#ifndef BME280_BOARD_CONFIG_H
#define BME280_BOARD_CONFIG_H

#include "em_gpio.h"

// I2C instance to use (0 or 1, depending on your board)
#define BME280_I2C_INSTANCE       0

// I2C pins - adjust these to match your hardware connections
// BRD4151A defaults: PC10 (SDA), PC11 (SCL)
#define BME280_I2C_SDA_PORT       gpioPortC
#define BME280_I2C_SDA_PIN        10

#define BME280_I2C_SCL_PORT       gpioPortC
#define BME280_I2C_SCL_PIN        11

// I2C configuration
#define BME280_I2C_FREQ           100000  // 100 kHz standard mode

// BME280 I2C address
// 0x76 if SDO pin is connected to GND (default)
// 0x77 if SDO pin is connected to VDDIO
#define BME280_I2C_ADDR           0x76

#endif // BME280_BOARD_CONFIG_H
