/**
 * @file bme280_min.h
 * @brief Minimal BME280 sensor driver
 *
 * Simplified driver for Bosch BME280 temperature, humidity, and pressure sensor.
 * Supports I2C interface only.
 */

#ifndef BME280_MIN_H
#define BME280_MIN_H

#include <stdint.h>
#include <stdbool.h>

// BME280 register addresses
#define BME280_REG_ID           0xD0
#define BME280_REG_RESET        0xE0
#define BME280_REG_CTRL_HUM     0xF2
#define BME280_REG_STATUS       0xF3
#define BME280_REG_CTRL_MEAS    0xF4
#define BME280_REG_CONFIG       0xF5
#define BME280_REG_PRESS_MSB    0xF7
#define BME280_REG_CALIB_00     0x88
#define BME280_REG_CALIB_26     0xE1

// BME280 / BMP280 chip IDs
#define BME280_CHIP_ID          0x60
#define BMP280_CHIP_ID          0x58

// Compensation parameters structure
typedef struct {
  uint16_t dig_T1;
  int16_t  dig_T2;
  int16_t  dig_T3;
  uint16_t dig_P1;
  int16_t  dig_P2;
  int16_t  dig_P3;
  int16_t  dig_P4;
  int16_t  dig_P5;
  int16_t  dig_P6;
  int16_t  dig_P7;
  int16_t  dig_P8;
  int16_t  dig_P9;
  uint8_t  dig_H1;
  int16_t  dig_H2;
  uint8_t  dig_H3;
  int16_t  dig_H4;
  int16_t  dig_H5;
  int8_t   dig_H6;
  int32_t  t_fine;  // Temperature fine value (used for compensation)
} bme280_calib_data_t;

// Measurement data structure
typedef struct {
  int32_t temperature;  // Temperature in 0.01 degrees Celsius
  uint32_t pressure;    // Pressure in Pa
  uint32_t humidity;    // Humidity in 0.01 %RH
} bme280_data_t;

/**
 * @brief Initialize BME280 sensor
 * @return true if successful, false otherwise
 */
bool bme280_init(void);

/**
 * @brief Read sensor data (temperature, pressure, humidity)
 * @param data Pointer to structure to store measurements
 * @return true if successful, false otherwise
 */
bool bme280_read_data(bme280_data_t *data);

/**
 * @brief Returns true if the detected sensor provides humidity.
 */
bool bme280_has_humidity(void);

/**
 * @brief Return detected chip ID (0 if not initialized).
 */
uint8_t bme280_get_chip_id(void);

/**
 * @brief Perform soft reset of the sensor
 * @return true if successful, false otherwise
 */
bool bme280_reset(void);

#endif // BME280_MIN_H
