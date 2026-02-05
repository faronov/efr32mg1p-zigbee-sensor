#include "hal/eeprom.h"
#include "em_gpio.h"
#include "sl_sleeptimer.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef EEPROM_SUCCESS
#define EEPROM_SUCCESS 0
#endif
#ifndef EEPROM_ERR
#define EEPROM_ERR 1
#endif
#ifndef EEPROM_ERR_INVALID_ADDR
#define EEPROM_ERR_INVALID_ADDR 2
#endif

#define SPI_FLASH_SIZE_BYTES (256u * 1024u)
#define SPI_FLASH_PAGE_SIZE 256u
#define SPI_FLASH_SECTOR_SIZE 4096u

#define FLASH_PORT_CS gpioPortB
#define FLASH_PIN_CS 11
#define FLASH_PORT_CLK gpioPortD
#define FLASH_PIN_CLK 13
#define FLASH_PORT_MISO gpioPortD
#define FLASH_PIN_MISO 14
#define FLASH_PORT_MOSI gpioPortD
#define FLASH_PIN_MOSI 15
#define FLASH_PORT_EN gpioPortF
#define FLASH_PIN_EN 3

static void flash_gpio_init(void)
{
  static bool configured = false;
  if (configured) {
    return;
  }

  GPIO_PinModeSet(FLASH_PORT_EN, FLASH_PIN_EN, gpioModePushPull, 1);
  GPIO_PinModeSet(FLASH_PORT_CS, FLASH_PIN_CS, gpioModePushPull, 1);
  GPIO_PinModeSet(FLASH_PORT_CLK, FLASH_PIN_CLK, gpioModePushPull, 0);
  GPIO_PinModeSet(FLASH_PORT_MOSI, FLASH_PIN_MOSI, gpioModePushPull, 0);
  GPIO_PinModeSet(FLASH_PORT_MISO, FLASH_PIN_MISO, gpioModeInput, 0);
  configured = true;
}

static inline void flash_cs_low(void)
{
  GPIO_PinOutClear(FLASH_PORT_CS, FLASH_PIN_CS);
}

static inline void flash_cs_high(void)
{
  GPIO_PinOutSet(FLASH_PORT_CS, FLASH_PIN_CS);
}

static uint8_t flash_bb_transfer(uint8_t out)
{
  uint8_t in = 0;
  for (int bit = 7; bit >= 0; bit--) {
    if (out & (1u << bit)) {
      GPIO_PinOutSet(FLASH_PORT_MOSI, FLASH_PIN_MOSI);
    } else {
      GPIO_PinOutClear(FLASH_PORT_MOSI, FLASH_PIN_MOSI);
    }

    for (volatile int d = 0; d < 20; d++) {
      __NOP();
    }

    GPIO_PinOutSet(FLASH_PORT_CLK, FLASH_PIN_CLK);
    if (GPIO_PinInGet(FLASH_PORT_MISO, FLASH_PIN_MISO)) {
      in |= (1u << bit);
    }

    for (volatile int d = 0; d < 20; d++) {
      __NOP();
    }

    GPIO_PinOutClear(FLASH_PORT_CLK, FLASH_PIN_CLK);
  }
  return in;
}

static uint8_t flash_read_status1(void)
{
  flash_cs_low();
  (void)flash_bb_transfer(0x05);
  uint8_t status = flash_bb_transfer(0x00);
  flash_cs_high();
  return status;
}

static bool flash_wait_ready(uint32_t timeout_ms)
{
  for (uint32_t i = 0; i < timeout_ms; i++) {
    if ((flash_read_status1() & 0x01u) == 0) {
      return true;
    }
    sl_sleeptimer_delay_millisecond(1);
  }
  return false;
}

static void flash_write_enable(void)
{
  flash_cs_low();
  (void)flash_bb_transfer(0x06);
  flash_cs_high();
}

static void flash_read(uint32_t address, uint8_t *data, uint16_t len)
{
  flash_cs_low();
  (void)flash_bb_transfer(0x03);
  (void)flash_bb_transfer((uint8_t)(address >> 16));
  (void)flash_bb_transfer((uint8_t)(address >> 8));
  (void)flash_bb_transfer((uint8_t)(address));
  for (uint16_t i = 0; i < len; i++) {
    data[i] = flash_bb_transfer(0x00);
  }
  flash_cs_high();
}

static bool flash_page_program(uint32_t address, const uint8_t *data, uint16_t len)
{
  flash_write_enable();
  flash_cs_low();
  (void)flash_bb_transfer(0x02);
  (void)flash_bb_transfer((uint8_t)(address >> 16));
  (void)flash_bb_transfer((uint8_t)(address >> 8));
  (void)flash_bb_transfer((uint8_t)(address));
  for (uint16_t i = 0; i < len; i++) {
    (void)flash_bb_transfer(data[i]);
  }
  flash_cs_high();
  return flash_wait_ready(5000);
}

static bool flash_sector_erase(uint32_t address)
{
  flash_write_enable();
  flash_cs_low();
  (void)flash_bb_transfer(0x20);
  (void)flash_bb_transfer((uint8_t)(address >> 16));
  (void)flash_bb_transfer((uint8_t)(address >> 8));
  (void)flash_bb_transfer((uint8_t)(address));
  flash_cs_high();
  return flash_wait_ready(5000);
}

uint8_t halEepromInit(void)
{
  flash_gpio_init();
  return EEPROM_SUCCESS;
}

uint8_t halEepromRead(uint32_t address, uint8_t *data, uint16_t len)
{
  if ((address + len) > SPI_FLASH_SIZE_BYTES) {
    return EEPROM_ERR_INVALID_ADDR;
  }
  flash_read(address, data, len);
  return EEPROM_SUCCESS;
}

uint8_t halEepromWrite(uint32_t address, uint8_t *data, uint16_t len)
{
  if ((address + len) > SPI_FLASH_SIZE_BYTES) {
    return EEPROM_ERR_INVALID_ADDR;
  }

  uint32_t offset = 0;
  while (offset < len) {
    uint32_t page_offset = (address + offset) % SPI_FLASH_PAGE_SIZE;
    uint16_t chunk = (uint16_t)(SPI_FLASH_PAGE_SIZE - page_offset);
    if (chunk > (len - offset)) {
      chunk = (uint16_t)(len - offset);
    }
    if (!flash_page_program(address + offset, &data[offset], chunk)) {
      return EEPROM_ERR;
    }
    offset += chunk;
  }

  return EEPROM_SUCCESS;
}

uint8_t halEepromErase(uint32_t address, uint32_t len)
{
  if ((address + len) > SPI_FLASH_SIZE_BYTES) {
    return EEPROM_ERR_INVALID_ADDR;
  }

  uint32_t start = address - (address % SPI_FLASH_SECTOR_SIZE);
  uint32_t end = address + len;
  if (end % SPI_FLASH_SECTOR_SIZE) {
    end += SPI_FLASH_SECTOR_SIZE - (end % SPI_FLASH_SECTOR_SIZE);
  }

  for (uint32_t addr = start; addr < end; addr += SPI_FLASH_SECTOR_SIZE) {
    if (!flash_sector_erase(addr)) {
      return EEPROM_ERR;
    }
  }

  return EEPROM_SUCCESS;
}

uint8_t halEepromBusy(void)
{
  return 0;
}

uint8_t halEepromShutdown(void)
{
  return EEPROM_SUCCESS;
}

const HalEepromInformationType *halEepromInfo(void)
{
  return NULL;
}

