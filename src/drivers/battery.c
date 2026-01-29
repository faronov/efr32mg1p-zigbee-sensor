/**
 * @file battery.c
 * @brief Battery voltage measurement for EFR32MG1P
 *
 * Measures battery voltage using internal ADC and VDD channel.
 * Configured for 2xAAA battery pack (nominal 3.0V).
 */

#include "battery.h"
#include "em_adc.h"
#include "em_cmu.h"

// Battery voltage thresholds for 2xAAA alkaline (in mV)
#define BATTERY_VOLTAGE_FULL_MV     3200  // 2x 1.6V fresh alkaline
#define BATTERY_VOLTAGE_NOMINAL_MV  3000  // 2x 1.5V nominal
#define BATTERY_VOLTAGE_EMPTY_MV    1800  // 2x 0.9V depleted

// ADC reference voltage in mV (internal 1.25V reference)
#define ADC_REF_VOLTAGE_MV          1250

// VDD divider: EFR32MG1P uses /3 divider for VDD measurement
#define VDD_DIVIDER                 3

/**
 * @brief Initialize battery voltage measurement
 */
bool battery_init(void)
{
  // Enable ADC clock
  CMU_ClockEnable(cmuClock_ADC0, true);

  // Initialize ADC for single conversion
  ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
  init.timebase = ADC_TimebaseCalc(0);
  init.prescale = ADC_PrescaleCalc(1000000, 0); // 1 MHz ADC clock
  ADC_Init(ADC0, &init);

  // Configure for single-ended mode
  ADC_InitSingle_TypeDef initSingle = ADC_INITSINGLE_DEFAULT;
  initSingle.reference = adcRef1V25;        // 1.25V internal reference
  initSingle.input = adcSingleInputVDDDiv3; // VDD/3 input
  initSingle.resolution = adcRes12Bit;      // 12-bit resolution
  initSingle.acqTime = adcAcqTime256;       // Longer acquisition for accuracy
  ADC_InitSingle(ADC0, &initSingle);

  return true;
}

/**
 * @brief Read battery voltage in millivolts
 */
uint16_t battery_read_voltage_mv(void)
{
  // Start ADC conversion
  ADC_Start(ADC0, adcStartSingle);

  // Wait for conversion to complete
  while (ADC0->STATUS & ADC_STATUS_SINGLEACT);

  // Read ADC result (12-bit)
  uint32_t adc_value = ADC_DataSingleGet(ADC0);

  // Calculate voltage in mV
  // VDD = (ADC_value / 4096) * ADC_REF_VOLTAGE * VDD_DIVIDER
  uint32_t voltage_mv = (adc_value * ADC_REF_VOLTAGE_MV * VDD_DIVIDER) / 4096;

  return (uint16_t)voltage_mv;
}

/**
 * @brief Read battery voltage in 100mV units (for Zigbee attribute)
 */
uint8_t battery_read_voltage_100mv(void)
{
  uint16_t voltage_mv = battery_read_voltage_mv();

  // Convert to 100mV units (e.g., 3000mV -> 30)
  uint8_t voltage_100mv = (uint8_t)(voltage_mv / 100);

  return voltage_100mv;
}

/**
 * @brief Calculate battery percentage remaining
 *
 * Uses linear interpolation between empty and full voltage.
 * Returns 0-200 value (0.5% resolution per Zigbee spec).
 */
uint8_t battery_calculate_percentage(uint16_t voltage_mv)
{
  // Clamp to valid range
  if (voltage_mv >= BATTERY_VOLTAGE_FULL_MV) {
    return 200; // 100%
  }
  if (voltage_mv <= BATTERY_VOLTAGE_EMPTY_MV) {
    return 0; // 0%
  }

  // Linear interpolation between empty and full
  // percentage = (current - empty) / (full - empty) * 200
  uint32_t voltage_range = BATTERY_VOLTAGE_FULL_MV - BATTERY_VOLTAGE_EMPTY_MV;
  uint32_t voltage_above_empty = voltage_mv - BATTERY_VOLTAGE_EMPTY_MV;
  uint32_t percentage = (voltage_above_empty * 200) / voltage_range;

  return (uint8_t)percentage;
}
