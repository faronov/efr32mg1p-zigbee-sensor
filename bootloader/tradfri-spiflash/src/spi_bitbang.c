#include "em_gpio.h"
#include <stdint.h>

// Bit-bang SPI driver for TRÅDFRI bootloader (IS25LQ020B).

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

static inline void bb_delay(void)
{
  for (volatile int d = 0; d < 20; d++) {
    __NOP();
  }
}

static uint8_t bb_transfer(uint8_t out)
{
  uint8_t in = 0;
  for (int bit = 7; bit >= 0; bit--) {
    if (out & (1u << bit)) {
      GPIO_PinOutSet(FLASH_PORT_MOSI, FLASH_PIN_MOSI);
    } else {
      GPIO_PinOutClear(FLASH_PORT_MOSI, FLASH_PIN_MOSI);
    }

    bb_delay();

    GPIO_PinOutSet(FLASH_PORT_CLK, FLASH_PIN_CLK);
    if (GPIO_PinInGet(FLASH_PORT_MISO, FLASH_PIN_MISO)) {
      in |= (1u << bit);
    }

    bb_delay();

    GPIO_PinOutClear(FLASH_PORT_CLK, FLASH_PIN_CLK);
  }
  return in;
}

void spi_init(void)
{
  GPIO_PinModeSet(FLASH_PORT_EN, FLASH_PIN_EN, gpioModePushPull, 1);
  GPIO_PinModeSet(FLASH_PORT_CS, FLASH_PIN_CS, gpioModePushPull, 1);
  GPIO_PinModeSet(FLASH_PORT_CLK, FLASH_PIN_CLK, gpioModePushPull, 0);
  GPIO_PinModeSet(FLASH_PORT_MOSI, FLASH_PIN_MOSI, gpioModePushPull, 0);
  GPIO_PinModeSet(FLASH_PORT_MISO, FLASH_PIN_MISO, gpioModeInput, 0);
}

void spi_deinit(void)
{
  GPIO_PinModeSet(FLASH_PORT_CS, FLASH_PIN_CS, gpioModeDisabled, 0);
  GPIO_PinModeSet(FLASH_PORT_CLK, FLASH_PIN_CLK, gpioModeDisabled, 0);
  GPIO_PinModeSet(FLASH_PORT_MOSI, FLASH_PIN_MOSI, gpioModeDisabled, 0);
  GPIO_PinModeSet(FLASH_PORT_MISO, FLASH_PIN_MISO, gpioModeDisabled, 0);
}

void spi_setCsActive(void)
{
  GPIO_PinOutClear(FLASH_PORT_CS, FLASH_PIN_CS);
}

void spi_setCsInactive(void)
{
  GPIO_PinOutSet(FLASH_PORT_CS, FLASH_PIN_CS);
}

void spi_writeByte(uint8_t byte)
{
  (void)bb_transfer(byte);
}

uint8_t spi_readByte(void)
{
  return bb_transfer(0xFF);
}

void spi_writeHalfword(uint16_t halfword)
{
  (void)bb_transfer((uint8_t)(halfword >> 8));
  (void)bb_transfer((uint8_t)(halfword & 0xFF));
}

uint16_t spi_readHalfword(void)
{
  uint16_t high = bb_transfer(0xFF);
  uint16_t low = bb_transfer(0xFF);
  return (uint16_t)((high << 8) | low);
}

void spi_write3Byte(uint32_t word)
{
  (void)bb_transfer((uint8_t)(word >> 16));
  (void)bb_transfer((uint8_t)(word >> 8));
  (void)bb_transfer((uint8_t)word);
}

uint32_t spi_getUsartPPUSATD(void)
{
  return 0;
}
