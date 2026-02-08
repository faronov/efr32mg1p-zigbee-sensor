#ifndef SHT31_H
#define SHT31_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int32_t temperature; // 0.01 C
  uint32_t humidity;   // 0.01 %RH
} sht31_data_t;

bool sht31_init(void);
bool sht31_read_data(sht31_data_t *data);
uint8_t sht31_get_i2c_addr(void);

#endif // SHT31_H
