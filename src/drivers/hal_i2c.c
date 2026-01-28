/**
 * @file hal_i2c.c
 * @brief I2C Hardware Abstraction Layer implementation for EFR32
 */

#include "hal_i2c.h"
#include "bme280_board_config.h"
#include "em_i2c.h"
#include "em_cmu.h"
#include "em_gpio.h"

// Determine which I2C instance to use
#if BME280_I2C_INSTANCE == 0
  #define I2C_PERIPHERAL    I2C0
  #define I2C_CLOCK         cmuClock_I2C0
#elif BME280_I2C_INSTANCE == 1
  #define I2C_PERIPHERAL    I2C1
  #define I2C_CLOCK         cmuClock_I2C1
#else
  #error "Invalid I2C instance"
#endif

static bool i2c_initialized = false;

bool hal_i2c_init(void)
{
  if (i2c_initialized) {
    return true;
  }

  // Enable clocks
  CMU_ClockEnable(cmuClock_HFPER, true);
  CMU_ClockEnable(I2C_CLOCK, true);
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Configure GPIO pins
  GPIO_PinModeSet(BME280_I2C_SDA_PORT, BME280_I2C_SDA_PIN, gpioModeWiredAndPullUp, 1);
  GPIO_PinModeSet(BME280_I2C_SCL_PORT, BME280_I2C_SCL_PIN, gpioModeWiredAndPullUp, 1);

  // Route I2C pins
  I2C_PERIPHERAL->ROUTEPEN = I2C_ROUTEPEN_SDAPEN | I2C_ROUTEPEN_SCLPEN;
  I2C_PERIPHERAL->ROUTELOC0 = (I2C_PERIPHERAL->ROUTELOC0 & ~(_I2C_ROUTELOC0_SDALOC_MASK | _I2C_ROUTELOC0_SCLLOC_MASK));

  // For EFR32MG1P, location routing depends on the specific pins
  // PC10/PC11 typically use location 14 or 15 on I2C0
  // This may need adjustment based on your exact part number
  #if BME280_I2C_INSTANCE == 0
    I2C_PERIPHERAL->ROUTELOC0 |= (14 << _I2C_ROUTELOC0_SDALOC_SHIFT) |
                                  (14 << _I2C_ROUTELOC0_SCLLOC_SHIFT);
  #endif

  // Initialize I2C
  I2C_Init_TypeDef i2cInit = I2C_INIT_DEFAULT;
  i2cInit.freq = BME280_I2C_FREQ;
  i2cInit.enable = true;

  I2C_Init(I2C_PERIPHERAL, &i2cInit);

  i2c_initialized = true;
  return true;
}

bool hal_i2c_write(uint8_t addr, const uint8_t *data, uint16_t len)
{
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;

  seq.addr = addr << 1;
  seq.flags = I2C_FLAG_WRITE;
  seq.buf[0].data = (uint8_t *)data;
  seq.buf[0].len = len;

  ret = I2C_TransferInit(I2C_PERIPHERAL, &seq);

  while (ret == i2cTransferInProgress) {
    ret = I2C_Transfer(I2C_PERIPHERAL);
  }

  return (ret == i2cTransferDone);
}

bool hal_i2c_read(uint8_t addr, uint8_t *data, uint16_t len)
{
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;

  seq.addr = addr << 1;
  seq.flags = I2C_FLAG_READ;
  seq.buf[0].data = data;
  seq.buf[0].len = len;

  ret = I2C_TransferInit(I2C_PERIPHERAL, &seq);

  while (ret == i2cTransferInProgress) {
    ret = I2C_Transfer(I2C_PERIPHERAL);
  }

  return (ret == i2cTransferDone);
}

bool hal_i2c_write_read(uint8_t addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;

  seq.addr = addr << 1;
  seq.flags = I2C_FLAG_WRITE_READ;
  seq.buf[0].data = &reg_addr;
  seq.buf[0].len = 1;
  seq.buf[1].data = data;
  seq.buf[1].len = len;

  ret = I2C_TransferInit(I2C_PERIPHERAL, &seq);

  while (ret == i2cTransferInProgress) {
    ret = I2C_Transfer(I2C_PERIPHERAL);
  }

  return (ret == i2cTransferDone);
}
