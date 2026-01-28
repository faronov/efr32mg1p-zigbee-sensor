/**
 * @file bme280_min.c
 * @brief Minimal BME280 sensor driver implementation
 *
 * Based on Bosch BME280 datasheet and compensation formulas.
 */

#include "bme280_min.h"
#include "hal_i2c.h"
#include "bme280_board_config.h"
#include <string.h>

static bme280_calib_data_t calib_data;
static bool sensor_initialized = false;

// Helper function to read register
static bool read_register(uint8_t reg, uint8_t *data, uint16_t len)
{
  return hal_i2c_write_read(BME280_I2C_ADDR, reg, data, len);
}

// Helper function to write register
static bool write_register(uint8_t reg, uint8_t value)
{
  uint8_t data[2] = {reg, value};
  return hal_i2c_write(BME280_I2C_ADDR, data, 2);
}

// Read calibration data from sensor
static bool read_calibration_data(void)
{
  uint8_t calib[32];

  // Read calibration data 0x88-0xA1 (26 bytes)
  if (!read_register(BME280_REG_CALIB_00, calib, 26)) {
    return false;
  }

  calib_data.dig_T1 = (uint16_t)(calib[1] << 8) | calib[0];
  calib_data.dig_T2 = (int16_t)(calib[3] << 8) | calib[2];
  calib_data.dig_T3 = (int16_t)(calib[5] << 8) | calib[4];

  calib_data.dig_P1 = (uint16_t)(calib[7] << 8) | calib[6];
  calib_data.dig_P2 = (int16_t)(calib[9] << 8) | calib[8];
  calib_data.dig_P3 = (int16_t)(calib[11] << 8) | calib[10];
  calib_data.dig_P4 = (int16_t)(calib[13] << 8) | calib[12];
  calib_data.dig_P5 = (int16_t)(calib[15] << 8) | calib[14];
  calib_data.dig_P6 = (int16_t)(calib[17] << 8) | calib[16];
  calib_data.dig_P7 = (int16_t)(calib[19] << 8) | calib[18];
  calib_data.dig_P8 = (int16_t)(calib[21] << 8) | calib[20];
  calib_data.dig_P9 = (int16_t)(calib[23] << 8) | calib[22];

  calib_data.dig_H1 = calib[25];

  // Read calibration data 0xE1-0xE7 (7 bytes)
  if (!read_register(BME280_REG_CALIB_26, calib, 7)) {
    return false;
  }

  calib_data.dig_H2 = (int16_t)(calib[1] << 8) | calib[0];
  calib_data.dig_H3 = calib[2];
  calib_data.dig_H4 = (int16_t)(calib[3] << 4) | (calib[4] & 0x0F);
  calib_data.dig_H5 = (int16_t)(calib[5] << 4) | (calib[4] >> 4);
  calib_data.dig_H6 = (int8_t)calib[6];

  return true;
}

// Compensate temperature (returns value in 0.01 degrees Celsius)
static int32_t compensate_temperature(int32_t adc_T)
{
  int32_t var1, var2, T;

  var1 = ((((adc_T >> 3) - ((int32_t)calib_data.dig_T1 << 1))) *
          ((int32_t)calib_data.dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)calib_data.dig_T1)) *
            ((adc_T >> 4) - ((int32_t)calib_data.dig_T1))) >> 12) *
          ((int32_t)calib_data.dig_T3)) >> 14;

  calib_data.t_fine = var1 + var2;
  T = (calib_data.t_fine * 5 + 128) >> 8;

  return T;  // Temperature in 0.01 degrees Celsius
}

// Compensate pressure (returns value in Pa)
static uint32_t compensate_pressure(int32_t adc_P)
{
  int64_t var1, var2, p;

  var1 = ((int64_t)calib_data.t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)calib_data.dig_P6;
  var2 = var2 + ((var1 * (int64_t)calib_data.dig_P5) << 17);
  var2 = var2 + (((int64_t)calib_data.dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)calib_data.dig_P3) >> 8) +
         ((var1 * (int64_t)calib_data.dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib_data.dig_P1) >> 33;

  if (var1 == 0) {
    return 0;  // Avoid division by zero
  }

  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)calib_data.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)calib_data.dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)calib_data.dig_P7) << 4);

  return (uint32_t)(p >> 8);  // Pressure in Pa (256th of Pa -> Pa)
}

// Compensate humidity (returns value in 0.01 %RH)
static uint32_t compensate_humidity(int32_t adc_H)
{
  int32_t v_x1_u32r;

  v_x1_u32r = (calib_data.t_fine - ((int32_t)76800));
  v_x1_u32r = (((((adc_H << 14) - (((int32_t)calib_data.dig_H4) << 20) -
                  (((int32_t)calib_data.dig_H5) * v_x1_u32r)) +
                 ((int32_t)16384)) >> 15) *
               (((((((v_x1_u32r * ((int32_t)calib_data.dig_H6)) >> 10) *
                    (((v_x1_u32r * ((int32_t)calib_data.dig_H3)) >> 11) +
                     ((int32_t)32768))) >> 10) +
                   ((int32_t)2097152)) *
                  ((int32_t)calib_data.dig_H2) + 8192) >> 14));

  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                             ((int32_t)calib_data.dig_H1)) >> 4));

  v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
  v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;

  uint32_t h = (uint32_t)(v_x1_u32r >> 12);
  return (h * 100) >> 10;  // Convert to 0.01 %RH
}

bool bme280_reset(void)
{
  return write_register(BME280_REG_RESET, 0xB6);
}

bool bme280_init(void)
{
  uint8_t chip_id;

  // Initialize I2C
  if (!hal_i2c_init()) {
    return false;
  }

  // Read and verify chip ID
  if (!read_register(BME280_REG_ID, &chip_id, 1)) {
    return false;
  }

  if (chip_id != BME280_CHIP_ID) {
    return false;  // Wrong chip ID
  }

  // Soft reset
  if (!bme280_reset()) {
    return false;
  }

  // Wait for reset to complete (typical 2ms)
  for (volatile int i = 0; i < 100000; i++);

  // Read calibration data
  if (!read_calibration_data()) {
    return false;
  }

  // Configure sensor
  // Humidity oversampling x1
  if (!write_register(BME280_REG_CTRL_HUM, 0x01)) {
    return false;
  }

  // Temperature oversampling x1, Pressure oversampling x1, Normal mode
  if (!write_register(BME280_REG_CTRL_MEAS, 0x27)) {
    return false;
  }

  // Standby time 1000ms, filter off
  if (!write_register(BME280_REG_CONFIG, 0xA0)) {
    return false;
  }

  sensor_initialized = true;
  return true;
}

bool bme280_read_data(bme280_data_t *data)
{
  uint8_t raw_data[8];
  int32_t adc_T, adc_P, adc_H;

  if (!sensor_initialized || data == NULL) {
    return false;
  }

  // Read all data registers (0xF7-0xFE)
  if (!read_register(BME280_REG_PRESS_MSB, raw_data, 8)) {
    return false;
  }

  // Extract raw ADC values
  adc_P = (int32_t)((((uint32_t)raw_data[0]) << 12) |
                    (((uint32_t)raw_data[1]) << 4) |
                    (((uint32_t)raw_data[2]) >> 4));

  adc_T = (int32_t)((((uint32_t)raw_data[3]) << 12) |
                    (((uint32_t)raw_data[4]) << 4) |
                    (((uint32_t)raw_data[5]) >> 4));

  adc_H = (int32_t)((((uint32_t)raw_data[6]) << 8) |
                    ((uint32_t)raw_data[7]));

  // Compensate values
  data->temperature = compensate_temperature(adc_T);
  data->pressure = compensate_pressure(adc_P);
  data->humidity = compensate_humidity(adc_H);

  return true;
}
