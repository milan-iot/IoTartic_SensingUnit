#ifndef _SENSORS_H
#define _SENSORS_H

#include <ArduinoJson.h>

/// Sensor types
typedef enum {AIR_TEMPERATURE = 0x00,
              AIR_HUMIDITY = 0x01,
              AIR_PRESSURE = 0x02,
              SOIL_TEMPERATURE_1 = 0x03,
              SOIL_TEMPERATURE_2 = 0x04,
              SOIL_MOISTURE_1 = 0x05,
              SOIL_MOISTURE_2 = 0x06,
              LUMINOSITY = 0x07,
              ERROR = -1
              } sensor_type;

/// Number of bytes for sensor header
#define HEADER_SIZE 1
/// Air temperature sensor header
#define AIR_TEMPERATURE_HEADER 0x00
/// Air humidity sensor header
#define AIR_HUMIDITY_HEADER 0x01
/// Air pressure sensor header
#define AIR_PRESSURE_HEADER 0x02
/// Soil temperature 1 sensor header
#define SOIL_TEMPERATURE_1_HEADER 0x03
/// Soil temperature 2 sensor header
#define SOIL_TEMPERATURE_2_HEADER 0x04
/// Soil moisture 1 sensor header
#define SOIL_MOISTURE_1_HEADER 0x05
/// Soil moisture 2 sensor header
#define SOIL_MOISTURE_2_HEADER 0x06
/// Luminosity sensor header
#define LUMINOSITY_HEADER 0x07

/// Number of bytes for air temperature sensor value
#define AIR_TEMPERATURE_SIZE 2
/// Number of bytes for air humidity sensor value
#define AIR_HUMIDITY_SIZE 2
/// Number of bytes for air pressure sensor value
#define AIR_PRESSURE_SIZE 4
/// Number of bytes for soil temperature 1 sensor value
#define SOIL_TEMPERATURE_1_SIZE 2
/// Number of bytes for soil temperature 2 sensor value
#define SOIL_TEMPERATURE_2_SIZE 2
/// Number of bytes for soil moisture 1 sensor value
#define SOIL_MOISTURE_1_SIZE 1
/// Number of bytes for soil moisture 2 sensor value
#define SOIL_MOISTURE_2_SIZE 1
/// Number of bytes for luminosity sensor value
#define LUMINOSITY_SIZE 2


/// Configuration structure for enabled sensors
typedef struct sensors_config
{
  // Number of bytes required for enabled sensors
  uint8_t number_of_sensors_bytes;

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
 * Function that resets sc strucutre, disableing all sensors
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
