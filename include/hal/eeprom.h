#ifndef HAL_EEPROM_H
#define HAL_EEPROM_H

#include <stdint.h>

#define EEPROM_SUCCESS 0
#define EEPROM_ERR 1
#define EEPROM_ERR_INVALID_ADDR 2

#define EEPROM_CAPABILITIES_ERASE_SUPPORTED 0x01
#define EEPROM_CAPABILITIES_PAGE_ERASE 0x02
#define EEPROM_CAPABILITIES_PARTIAL_ERASE 0x04

typedef struct {
  uint8_t version;
  uint32_t partSize;
  uint16_t pageSize;
  uint8_t erasedMemValue;
  uint8_t capabilitiesMask;
  uint16_t pageEraseMs;
  uint16_t partEraseMs;
} HalEepromInformationType;

uint8_t halEepromInit(void);
uint8_t halEepromRead(uint32_t address, uint8_t *data, uint16_t len);
uint8_t halEepromWrite(uint32_t address, uint8_t *data, uint16_t len);
uint8_t halEepromErase(uint32_t address, uint32_t len);
uint8_t halEepromBusy(void);
uint8_t halEepromShutdown(void);
const HalEepromInformationType *halEepromInfo(void);

#endif // HAL_EEPROM_H
