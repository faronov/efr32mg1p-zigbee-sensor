/**
 * @file sht31.c
 * @brief Minimal SHT31 I2C driver (temperature + humidity).
 */

#include "sht31.h"
#include "hal_i2c.h"
#include "sl_sleeptimer.h"
#include <string.h>

#define SHT31_ADDR_PRIMARY   0x44
#define SHT31_ADDR_SECONDARY 0x45

#define SHT31_CMD_SOFT_RESET_MSB 0x30
#define SHT31_CMD_SOFT_RESET_LSB 0xA2
#define SHT31_CMD_MEASURE_HPM_MSB 0x24
#define SHT31_CMD_MEASURE_HPM_LSB 0x00

static uint8_t detected_addr = 0;

static uint8_t crc8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

static bool try_measure(uint8_t addr, sht31_data_t *data)
{
  uint8_t cmd[2] = { SHT31_CMD_MEASURE_HPM_MSB, SHT31_CMD_MEASURE_HPM_LSB };
  uint8_t rx[6];
  uint16_t raw_t;
  uint16_t raw_h;
  int32_t temp_centi;
  uint32_t hum_centi;

  if (!hal_i2c_write(addr, cmd, sizeof(cmd))) {
    return false;
  }

  // High repeatability measurement max conversion time is ~15 ms.
  sl_sleeptimer_delay_millisecond(20);

  if (!hal_i2c_read(addr, rx, sizeof(rx))) {
    return false;
  }

  if (crc8(&rx[0], 2) != rx[2] || crc8(&rx[3], 2) != rx[5]) {
    return false;
  }

  raw_t = (uint16_t)((rx[0] << 8) | rx[1]);
  raw_h = (uint16_t)((rx[3] << 8) | rx[4]);

  temp_centi = -4500 + ((int32_t)raw_t * 17500) / 65535;
  hum_centi = ((uint32_t)raw_h * 10000u) / 65535u;
  if (hum_centi > 10000u) {
    hum_centi = 10000u;
  }

  data->temperature = temp_centi;
  data->humidity = hum_centi;
  return true;
}

bool sht31_init(void)
{
  uint8_t cmd_reset[2] = { SHT31_CMD_SOFT_RESET_MSB, SHT31_CMD_SOFT_RESET_LSB };
  sht31_data_t sample;

  detected_addr = 0;
  (void)hal_i2c_init();

  // Soft reset may fail if another sensor is on the bus; we still try measurement.
  (void)hal_i2c_write(SHT31_ADDR_PRIMARY, cmd_reset, sizeof(cmd_reset));
  (void)hal_i2c_write(SHT31_ADDR_SECONDARY, cmd_reset, sizeof(cmd_reset));
  sl_sleeptimer_delay_millisecond(2);

  if (try_measure(SHT31_ADDR_PRIMARY, &sample)) {
    detected_addr = SHT31_ADDR_PRIMARY;
    return true;
  }

  if (try_measure(SHT31_ADDR_SECONDARY, &sample)) {
    detected_addr = SHT31_ADDR_SECONDARY;
    return true;
  }

  return false;
}

bool sht31_read_data(sht31_data_t *data)
{
  if (data == NULL || detected_addr == 0) {
    return false;
  }
  return try_measure(detected_addr, data);
}

uint8_t sht31_get_i2c_addr(void)
{
  return detected_addr;
}
