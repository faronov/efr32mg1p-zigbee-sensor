/**
 * @file hal_i2c.h
 * @brief I2C Hardware Abstraction Layer for Silicon Labs EFR32
 *
 * Provides simple I2C read/write functions using Silicon Labs EMLIB.
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize I2C peripheral
 * @return true if successful, false otherwise
 */
bool hal_i2c_init(void);

/**
 * @brief Write data to I2C device
 * @param addr 7-bit I2C device address
 * @param data Pointer to data buffer
 * @param len Number of bytes to write
 * @return true if successful, false otherwise
 */
bool hal_i2c_write(uint8_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief Read data from I2C device
 * @param addr 7-bit I2C device address
 * @param data Pointer to receive buffer
 * @param len Number of bytes to read
 * @return true if successful, false otherwise
 */
bool hal_i2c_read(uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Write to register then read response (common I2C pattern)
 * @param addr 7-bit I2C device address
 * @param reg_addr Register address to read from
 * @param data Pointer to receive buffer
 * @param len Number of bytes to read
 * @return true if successful, false otherwise
 */
bool hal_i2c_write_read(uint8_t addr, uint8_t reg_addr, uint8_t *data, uint16_t len);

#endif // HAL_I2C_H
