#include <Arduino.h>
#include <DallasTemperature.h>
#include <BH1750FVI.h>
#include <Wire.h>
#include <SparkFunBME280.h>
#include <ArduinoJson.h>
#include "sensors.h"

// DS18B20 soil temperature 
OneWire DS18B20_onewire_1(18);
OneWire DS18B20_onewire_2(19);
DallasTemperature DS18B20_1(&DS18B20_onewire_1);
DallasTemperature DS18B20_2(&DS18B20_onewire_2);

// Soil moisture sensors
#define SOIL_IN_1  35
#define SOIL_IN_2  4
#define SOIL_DRY 4096
#define SOIL_WET 0

// BH1750FVI
BH1750FVI::eDeviceAddress_t DEVICEADDRESS = BH1750FVI::k_DevAddress_L;
BH1750FVI::eDeviceMode_t DEVICEMODE = BH1750FVI::k_DevModeContHighRes;
BH1750FVI LightSensor(23, DEVICEADDRESS, DEVICEMODE);

// BME280
BME280 bme280; //Uses I2C address 0x76 (jumper closed)

void resetSensorConfig(sensors_config *sc)
{
  sc->air_temp = false;
  sc->air_hum = false;
  sc->air_pres = false;

  sc->soil_temp_1 = false;
  sc->soil_temp_2 =false;
  sc->soil_moist_1 =false;
  sc->soil_moist_2 = false;

  sc->lum = false;

  sc->number_of_sensors_bytes = 0;
}

uint8_t calculateNumberOfSensorsBytes(sensors_config *sc)
{
  sc->number_of_sensors_bytes = 0;

  if(sc->air_temp)
    sc->number_of_sensors_bytes += HEADER_SIZE + AIR_TEMPERATURE_SIZE;
  if(sc->air_hum)
    sc->number_of_sensors_bytes += HEADER_SIZE + AIR_HUMIDITY_SIZE;
  if(sc->air_pres)  
    sc->number_of_sensors_bytes += HEADER_SIZE + AIR_PRESSURE_SIZE;
  
  if(sc->soil_temp_1)
    sc->number_of_sensors_bytes += HEADER_SIZE + SOIL_TEMPERATURE_1_SIZE;
  if(sc->soil_temp_2)
    sc->number_of_sensors_bytes += HEADER_SIZE + SOIL_TEMPERATURE_2_SIZE;
  if(sc->soil_moist_1)
    sc->number_of_sensors_bytes += HEADER_SIZE + SOIL_MOISTURE_1_SIZE;
  if(sc->soil_moist_2)
    sc->number_of_sensors_bytes += HEADER_SIZE + SOIL_MOISTURE_2_SIZE;

  if(sc->lum)
    sc->number_of_sensors_bytes += HEADER_SIZE + LUMINOSITY_SIZE;

  return sc->number_of_sensors_bytes;
}

void initSensors(sensors_config *sc)
{
  //sensor init
  if(sc->soil_temp_1)
    DS18B20_1.begin();
  if(sc->soil_temp_2)
    DS18B20_2.begin();
  if(sc->lum)
    LightSensor.begin();

  if(sc->air_hum || sc->air_temp || sc->air_pres) 
  {
    Wire.begin();
    bme280.setI2CAddress(0x76); //Connect to a second sensor
    if(bme280.beginI2C() == false) 
      Serial.println("BME280 connect failed");
  }
}

/**
 * Function that measures soil moisture
 * @param sms - Numeber of soil moisture sensor (SOIL_MOISTURE_1 or SOIL_MOISTURE_2)
 * @return Scaled soil moisture sensor value
 **/
int16_t soil_moisture(sensor_type sms)
{
  int16_t moist, val;
  if (sms == SOIL_MOISTURE_1)
    val = analogRead(SOIL_IN_1);
  else
    val = analogRead(SOIL_IN_2);

  if (val < SOIL_WET)
    moist = 100;
  else if (val > SOIL_DRY)
    moist = 0;
  else
    moist = ((SOIL_DRY - val) * 100) / (SOIL_DRY - SOIL_WET);
  
  return moist;
}

void getSensorData(sensor_data *sd, sensors_config *sc)
{
  // BME280
  if(sc->air_temp)
    sd->air_temp = bme280.readTempC() * 100;
  if(sc->air_hum)
    sd->air_hum = bme280.readFloatHumidity() * 100;
  if(sc->air_pres)  
    sd->air_pres = bme280.readFloatPressure();

  // DS18B20
  if(sc->soil_temp_1)
  {
    DS18B20_1.requestTemperatures();
    sd->soil_temp_1 = DS18B20_1.getTempCByIndex(0) * 100;
  }
  if(sc->soil_temp_2)
  {
    DS18B20_2.requestTemperatures();
    sd->soil_temp_2 = DS18B20_2.getTempCByIndex(0) * 100;
  }

  // Soil moisture
  if(sc->soil_moist_1)
    sd->soil_moist_1 = soil_moisture(SOIL_MOISTURE_1);
  if(sc->soil_moist_2)
    sd->soil_moist_2 = soil_moisture(SOIL_MOISTURE_2);

  //BH1750FVI
  if(sc->lum)
    sd->lum = LightSensor.GetLightIntensity();
}

bool convertToSensorDataArray(uint8_t *data, uint16_t length, uint16_t *size, sensor_data *sd, sensors_config *sc)
{
  *size = 0;

  if(sc->air_temp)
  {
    if((*size) + HEADER_SIZE + AIR_TEMPERATURE_SIZE < length)
    {
      data[(*size)++] = AIR_TEMPERATURE_HEADER;
      memcpy(&data[*size], (uint8_t *)&sd->air_temp, 2);
      *size += AIR_TEMPERATURE_SIZE;
    }
    else
      return false;
  }

  if(sc->air_hum)
  {
    if((*size) + HEADER_SIZE + AIR_HUMIDITY_SIZE < length)
    {
      data[(*size)++] = AIR_HUMIDITY_HEADER;
      memcpy(&data[*size], (uint8_t *)&sd->air_hum, 2);
      *size += AIR_HUMIDITY_SIZE;
    }
    else
      return false;
  }

  if(sc->air_pres)
  {
    if((*size) + HEADER_SIZE + AIR_PRESSURE_SIZE < length)
    {
      data[(*size)++] = AIR_PRESSURE_HEADER;
      memcpy(&data[*size], (uint8_t *)&sd->air_pres, 4);
      *size += AIR_PRESSURE_SIZE;
    }
    else
      return false;
  }

  if(sc->soil_temp_1)
  {
    if((*size) + HEADER_SIZE + SOIL_TEMPERATURE_1_SIZE < length)
    {
      data[(*size)++] = SOIL_TEMPERATURE_1_HEADER;
      memcpy(&data[*size], (uint8_t *)&sd->soil_temp_1, 2);
      *size += SOIL_TEMPERATURE_1_SIZE;
    }
    else
      return false;
  }

  if(sc->soil_temp_2)
  {
    if((*size) + HEADER_SIZE + SOIL_TEMPERATURE_2_SIZE < length)
    {
      data[(*size)++] = SOIL_TEMPERATURE_2_HEADER;
      memcpy(&data[*size], (uint8_t *)&sd->soil_temp_2, 2);
      *size += SOIL_TEMPERATURE_2_SIZE;
    }
    else
      return false;
  }

  if(sc->soil_moist_1)
  {
    if((*size) + HEADER_SIZE + SOIL_MOISTURE_1_SIZE < length)
    {
      data[(*size)++] = SOIL_MOISTURE_1_HEADER;
      memcpy(&data[*size], (uint8_t *)&sd->soil_moist_1, 1);
      *size += SOIL_MOISTURE_1_SIZE;
    }
    else
      return false;
  }

  if(sc->soil_moist_2)
  {
    if((*size) + HEADER_SIZE + SOIL_MOISTURE_2_SIZE < length)
    {
      data[(*size)++] = SOIL_MOISTURE_2_HEADER;
      memcpy(&data[*size], (uint8_t *)&sd->soil_moist_2, 1);
      *size += SOIL_MOISTURE_2_SIZE;
    }
    else
      return false;
  }

  if(sc->lum)
  {
    if((*size) + HEADER_SIZE + LUMINOSITY_SIZE < length)
    {
      data[(*size)++] = LUMINOSITY_HEADER;
      memcpy(&data[*size], (uint8_t *)&sd->lum, 2);
      *size += LUMINOSITY_SIZE;
    }
    else
      return false;
  }

  return (*size != 0);
}

/**
 * Function that parses sensor value and enables sensor in configuration
 * @param sd - Sensor data structure in which value will be stored
 * @param sc - Sensor configuration in which sensor will be enabled
 * @param data - Array that contains header and value of sensor
 * @param size - Size of array
 * @param position - Position of targeted sensor value in array that should be parsed
 * @return Returns true on successful parse
 **/
bool parseSensorValue(sensor_data *sd, sensors_config *sc, const uint8_t *data, const uint16_t size, uint8_t *position)
{
  switch(data[0])
  {
    case AIR_TEMPERATURE:
      if(!sc->air_temp && (HEADER_SIZE + AIR_TEMPERATURE_SIZE <= size))
      {
        sc->air_temp = true;
        sd->air_temp = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + AIR_TEMPERATURE_SIZE;
        return true;
      }
      else
        return false;
    case AIR_HUMIDITY:
      if(!sc->air_hum && (HEADER_SIZE + AIR_HUMIDITY_SIZE <= size))
      {
        sc->air_hum = true;
        sd->air_hum = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + AIR_HUMIDITY_SIZE;
        return true;
      }
      else
        return false;
    case AIR_PRESSURE:
      if(!sc->air_pres && (HEADER_SIZE + AIR_PRESSURE_SIZE <= size))
      {
        sc->air_pres = true;
        sd->air_pres = ((uint32_t)data[4] << 24) | ((uint32_t)data[3] << 16) | ((uint32_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + AIR_PRESSURE_SIZE;
        return true;
      }
      else
        return false;
    case SOIL_TEMPERATURE_1:
      if(!sc->soil_temp_1 && (HEADER_SIZE + SOIL_TEMPERATURE_1_SIZE <= size))
      {
        sc->soil_temp_1 = true;
        sd->soil_temp_1 = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + SOIL_TEMPERATURE_1_SIZE;
        return true;
      }
      else
        return false;
    case SOIL_TEMPERATURE_2:
      if(!sc->soil_temp_2 && (HEADER_SIZE + SOIL_TEMPERATURE_2_SIZE <= size))
      {
        sc->soil_temp_2 = true;
        sd->soil_temp_2 = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + SOIL_TEMPERATURE_2_SIZE;
        return true;
      }
      else
        return false;
    case SOIL_MOISTURE_1:
      if(!sc->soil_moist_1 && (HEADER_SIZE + SOIL_TEMPERATURE_1_SIZE <= size))
      {
        sc->soil_moist_1 = true;
        sd->soil_moist_1 = data[1];
        *position += HEADER_SIZE + SOIL_TEMPERATURE_1_SIZE;
        return true;
      }
      else
        return false;
    case SOIL_MOISTURE_2:
      if(!sc->soil_moist_2 && (HEADER_SIZE + SOIL_MOISTURE_2_SIZE <= size))
      {
        sc->soil_moist_2 = true;
        sd->soil_moist_2 = data[1];
        *position += HEADER_SIZE + SOIL_MOISTURE_2_SIZE;
        return true;
      }
      else
        return false;
    case LUMINOSITY:
      if(!sc->lum && (HEADER_SIZE + LUMINOSITY_SIZE <= size))
      {
        sc->lum = true;
        sd->lum = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + LUMINOSITY_SIZE;
        return true;
      }
      else
        return false;
    default:
      return false;
  }
}

bool convertToSensorData(sensor_data *sd, sensors_config *sc, const uint8_t *data, const uint16_t size)
{
  uint8_t position = 0;

  resetSensorConfig(sc);

  while(position < size)
  {
    if(!parseSensorValue(sd, sc, &data[position], size - position, &position))
      return false;
  }

  sc->number_of_sensors_bytes = position;

  return true;
}

void printSensorData(sensor_data *sd, sensors_config *sc)
{  
  //BH1750FVI
  if(sc->lum)
  {
    Serial.println("*** BH1750FVI ***");
    Serial.println("\tL = " + (String)(sd->lum) + " lux");
  }
  
  // DS18B20
  if(sc->soil_temp_1 || sc->soil_temp_2)
    Serial.println("*** DS18B20 ***");
  if(sc->soil_temp_1)
    Serial.println("\tT1 = " + (String)(sd->soil_temp_1 / 100) + "." + (String)(sd->soil_temp_1 % 100) + " C");
  if(sc->soil_temp_2)
    Serial.println("\tT2 = " + (String)(sd->soil_temp_2 / 100) + "." + (String)(sd->soil_temp_2 % 100) + " C");

  // Soil moisture
  if(sc->soil_moist_1 || sc->soil_moist_2)
    Serial.println("*** Soil moisture ***");
  if(sc->soil_moist_1)
    Serial.println("\tH1 = " + (String)sd->soil_moist_1 + " %");
  if(sc->soil_moist_2)
    Serial.println("\tH2 = " + (String)sd->soil_moist_2 + " %");

  // BME280
  if(sc->air_hum || sc->air_temp || sc->air_pres)
    Serial.println("*** BME280 ***");
  if(sc->air_hum)
    Serial.println("\tH = " + String(sd->air_hum / 100) + "." + (String)(sd->air_hum % 100) + " \%");
  if(sc->air_temp)
    Serial.println("\tT = " + String(sd->air_temp / 100) + "." + (String)(sd->air_temp % 100) + " C");
  if(sc->air_pres)
    Serial.println("\tP = " + String(sd->air_pres / 100) + "." + (String)(sd->air_pres % 100) + " mBar");
}