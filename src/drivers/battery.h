/**
 * @file battery.h
 * @brief Battery voltage measurement for EFR32MG1P
 *
 * Provides functions to measure battery voltage using internal ADC
 * and calculate battery percentage for 2xAAA configuration.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize battery voltage measurement
 *
 * Initializes the ADC for measuring VDD (supply voltage).
 *
 * @return true if initialization successful, false otherwise
 */
bool battery_init(void);

/**
 * @brief Read battery voltage
 *
 * Measures the current battery voltage using internal ADC.
 *
 * @return Battery voltage in millivolts (mV), or 0 on error
 */
uint16_t battery_read_voltage_mv(void);

/**
 * @brief Read battery voltage in 100mV units
 *
 * Measures battery voltage and returns it in 100mV units
 * as required by Zigbee Power Configuration Cluster.
 *
 * @return Battery voltage in 100mV units (e.g., 30 = 3.0V), or 0 on error
 */
uint8_t battery_read_voltage_100mv(void);

/**
 * @brief Get last averaged raw ADC sample used for battery conversion.
 *
 * @return 12-bit ADC code (0-4095)
 */
uint16_t battery_get_last_raw_adc(void);

/**
 * @brief Indicates whether last battery sample was valid.
 *
 * @return true when last conversion looked sane; false when fallback was used
 */
bool battery_last_measurement_valid(void);

/**
 * @brief Calculate battery percentage remaining
 *
 * Calculates battery percentage based on voltage curve for 2xAAA batteries.
 * Uses 0-200 scale with 0.5% resolution as per Zigbee spec.
 *
 * Voltage thresholds for 2xAAA (alkaline):
 * - Full: 3.2V (2x 1.6V fresh)
 * - Nominal: 3.0V (2x 1.5V)
 * - Empty: 1.8V (2x 0.9V depleted)
 *
 * @param voltage_mv Battery voltage in millivolts
 * @return Battery percentage (0-200, where 200 = 100%, 100 = 50%, 0 = 0%)
 */
uint8_t battery_calculate_percentage(uint16_t voltage_mv);

#endif // BATTERY_H
