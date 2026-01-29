/**
 * @file bme280_board_config_tradfri.h
 * @brief BME280 I2C pin configuration for IKEA TRÅDFRI board
 *
 * IKEA TRÅDFRI board (EFR32MG1P132F256GM32) pinout:
 * - I2C available on PC10 (SDA) and PC11 (SCL)
 * - BME280 should be connected to these pins
 */

#ifndef BME280_BOARD_CONFIG_TRADFRI_H
#define BME280_BOARD_CONFIG_TRADFRI_H

#include "em_gpio.h"
#include "em_cmu.h"

// I2C peripheral configuration for TRÅDFRI
#define BME280_I2C_PERIPHERAL         I2C0
#define BME280_I2C_CLOCK              cmuClock_I2C0
#define BME280_I2C_FREQ               100000  // 100 kHz for better noise immunity

// I2C SDA pin - PC10 on TRÅDFRI (available on connector)
#define BME280_I2C_SDA_PORT           gpioPortC
#define BME280_I2C_SDA_PIN            10

// I2C SCL pin - PC11 on TRÅDFRI (available on connector)
#define BME280_I2C_SCL_PORT           gpioPortC
#define BME280_I2C_SCL_PIN            11

// I2C routing location for EFR32MG1P (Series 1)
// Location 14 for I2C0: SDA on PC10, SCL on PC11
#define BME280_I2C_ROUTE_LOCATION     14

// BME280 I2C address (SDO pin to GND = 0x76, SDO to VCC = 0x77)
#define BME280_I2C_ADDR               0x76

/**
 * TRÅDFRI Pin Summary:
 *
 * Available GPIO on connector:
 * - PA0, PA1: LED/General GPIO (we use PA0 for LED)
 * - PB12, PB13: Button/General GPIO (we use PB13 for Button)
 * - PB14, PB15: Additional GPIO (available for expansion)
 * - PC10, PC11: I2C (used for BME280)
 * - PF0, PF1, PF2: SWD Debug (SWCLK, SWDIO, SWO)
 *
 * SPI Flash (IS25LQ020B) pins:
 * - PB11: CS
 * - PD13: CLK
 * - PD14: MISO
 * - PD15: MOSI
 *
 * BME280 Connection:
 * VCC  -> 3.3V
 * GND  -> GND
 * SDA  -> PC10 (requires 4.7kΩ pull-up to 3.3V)
 * SCL  -> PC11 (requires 4.7kΩ pull-up to 3.3V)
 * SDO  -> GND (for address 0x76) or 3.3V (for address 0x77)
 */

#endif // BME280_BOARD_CONFIG_TRADFRI_H
