#ifndef _SENSORS_H
#define _SENSORS_H

#include <ArduinoJson.h>

/// Sensor types
typedef enum {
  AIR_TEMPERATURE = 0x00,
  AIR_HUMIDITY = 0x01,
  AIR_PRESSURE = 0x02,
  SOIL_TEMPERATURE_1 = 0x03,
  SOIL_TEMPERATURE_2 = 0x04,
  SOIL_MOISTURE_1 = 0x05,
  SOIL_MOISTURE_2 = 0x06,
  LUMINOSITY = 0x07,
  ERROR = -1
} sensor_type;

/// Sensor ICs
typedef enum {
  IC_BME280 = 1, IC_BME680
} air_sensor_ic;

/// Number of bytes for sensor header
#define HEADER_SIZE 1

//https://stackoverflow.com/questions/3553296/sizeof-single-struct-member-in-c
#define member_size(type, member) sizeof(((type *)0)->member)

/// Configuration structure for enabled sensors
typedef struct sensors_config
{
  // Number of bytes required for enabled sensors
  uint8_t number_of_sensors_bytes;

  //air sensor IC
  air_sensor_ic air_sensor;

  // Air parameters
  bool air_temp;
  bool air_hum;
  bool air_pres;

  // Soil parameters
  bool soil_temp_1;
  bool soil_temp_2;
  bool soil_moist_1;
  bool soil_moist_2;

  // Luminosity
  bool lum;
} sensors_config;

/// Data structure for enabled sensor values
typedef struct sensor_data
{
  // Air parameters
  int16_t air_temp;
  int16_t air_hum;
  int32_t air_pres;

  // Soil parameters
  int16_t soil_temp_1;
  int16_t soil_temp_2;
  int8_t soil_moist_1;
  int8_t soil_moist_2;

  // Luminosity
  uint16_t lum;
} sensor_data;

/**
 * Function that resets sc strucutre, disabling all sensors
 * @param sc - Sensor configuration structure
 * @return No return value
 **/
void resetSensorConfig(sensors_config *sc);

/**
 * Function that calculates number of bytes required for representation of values for enabled sensors
 * @param sc - Sensor configuration
 * @return Number of required bytes for enabled sensors
 **/
uint8_t calculateNumberOfSensorsBytes(sensors_config *sc);

/**
 * Function that initializes enabled sensors
 * @param sc - Sensor configuration
 * @return No return value
 **/
void initSensors(sensors_config *sc);

/**
 * Function measures values of enabled sensors
 * @param sd - Sensor data structure in which measurements are stored
 * @param sc - Sensor configuration
 * @return No return value
 **/
void getSensorData(sensor_data *sd, sensors_config *sc);

/**
 * Function that converts sensor_data structure to array of values with headers for enabled sensors
 * @param data - Array to be created
 * @param length - Maximum length of array
 * @param size - Size of created array
 * @param sd - Sensor data structure in which measurements are stored
 * @param sc - Sensor configuration
 * @return Returns true on successful conversion
 **/
bool convertToSensorDataArray(uint8_t *data, uint16_t length, uint16_t *size, sensor_data *sd, sensors_config *sc);

/**
 * Function that creates configuration and parses sensor values from array
 * @param sd - Sensor data structure in which values will be stored
 * @param sc - Sensor configuration that will be creadted
 * @param data - Array that contains headers and values of sensors
 * @param size - Size of array
 * @return Returns true on successful conversion
 **/
bool convertToSensorData(sensor_data *sd, sensors_config *sc, const uint8_t *data, const uint16_t size);

/**
 * Function that prints sensor values enabled for device
 * @param sd - Sensor data structure in which values are stored
 * @param sc - Sensor configuration
 * @return No return value
 **/
void printSensorData(sensor_data *sd, sensors_config *sc);

#endif
