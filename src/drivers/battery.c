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

// ADC reference voltage in mV defaults.
#define ADC_REF_VOLTAGE_1V25_MV     1250
#define ADC_REF_VOLTAGE_5V_MV       5000

// AVDD gain: EFR32MG1P Series 1 uses 1/4 gain for AVDD measurement
#define AVDD_SCALE_FACTOR           4

// ADC sanity limits to reject obvious bad reads.
#define BATTERY_MIN_VALID_MV        1200
#define BATTERY_MAX_VALID_MV        3600

static bool battery_adc_ready = false;
static uint16_t battery_last_raw_adc = 0;
static bool battery_last_valid = false;
static uint16_t battery_last_good_mv = BATTERY_VOLTAGE_NOMINAL_MV;
static uint16_t battery_ref_mv = ADC_REF_VOLTAGE_1V25_MV;
static uint8_t battery_scale_factor = AVDD_SCALE_FACTOR;

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
  // On some MG1 boards AVDD measurement saturates with 1.25V ref.
  // Prefer 5V ref when available to keep measurement in range.
#if defined(_ADC_SINGLECTRL_REF_5V)
  initSingle.reference = adcRef5V;
  battery_ref_mv = ADC_REF_VOLTAGE_5V_MV;
  battery_scale_factor = 1;
#else
  initSingle.reference = adcRef1V25;        // 1.25V internal reference
  battery_ref_mv = ADC_REF_VOLTAGE_1V25_MV;
  battery_scale_factor = AVDD_SCALE_FACTOR;
#endif
  initSingle.posSel = adcPosSelAVDD;        // VDD measurement (AVDD channel)
  initSingle.resolution = adcRes12Bit;      // 12-bit resolution
  initSingle.acqTime = adcAcqTime256;       // Longer acquisition for accuracy
  ADC_InitSingle(ADC0, &initSingle);

  battery_adc_ready = true;
  return true;
}

/**
 * @brief Read battery voltage in millivolts
 */
uint16_t battery_read_voltage_mv(void)
{
  if (!battery_adc_ready) {
    battery_last_valid = false;
    return battery_last_good_mv;
  }

  // Average several samples and wait on conversion-complete flag
  // to avoid stale zero reads on some MG1 boards.
  uint32_t sum = 0;
  const uint8_t samples = 4;
  for (uint8_t i = 0; i < samples; i++) {
    ADC_IntClear(ADC0, ADC_IF_SINGLE);
    ADC_Start(ADC0, adcStartSingle);
    while ((ADC_IntGet(ADC0) & ADC_IF_SINGLE) == 0u) {
      // wait
    }
    sum += ADC_DataSingleGet(ADC0) & 0x0FFFu;
  }

  uint32_t adc_value = sum / samples;
  battery_last_raw_adc = (uint16_t)adc_value;

  // Calculate voltage in mV
  // AVDD = (ADC_value / 4095) * Vref * scale
  uint32_t voltage_mv = (adc_value * battery_ref_mv * battery_scale_factor) / 4095u;

  if (voltage_mv >= BATTERY_MIN_VALID_MV && voltage_mv <= BATTERY_MAX_VALID_MV) {
    battery_last_valid = true;
    battery_last_good_mv = (uint16_t)voltage_mv;
    return (uint16_t)voltage_mv;
  }

  battery_last_valid = false;
  return battery_last_good_mv;
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

uint16_t battery_get_last_raw_adc(void)
{
  return battery_last_raw_adc;
}

bool battery_last_measurement_valid(void)
{
  return battery_last_valid;
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
