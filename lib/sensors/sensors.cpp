#include <Arduino.h>
#include <DallasTemperature.h>
#include <BH1750FVI.h>
#include <Wire.h>
#include <SparkFunBME280.h>
#include <Zanshin_BME680.h>
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
BME680_Class BME680;

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
    sc->number_of_sensors_bytes += HEADER_SIZE + member_size(sensor_data, air_temp);
  if(sc->air_hum)
    sc->number_of_sensors_bytes += HEADER_SIZE + member_size(sensor_data, air_hum);
  if(sc->air_pres)  
    sc->number_of_sensors_bytes += HEADER_SIZE + member_size(sensor_data, air_pres);
  
  if(sc->soil_temp_1)
    sc->number_of_sensors_bytes += HEADER_SIZE + member_size(sensor_data, soil_temp_1);
  if(sc->soil_temp_2)
    sc->number_of_sensors_bytes += HEADER_SIZE + member_size(sensor_data, soil_temp_2);
  if(sc->soil_moist_1)
    sc->number_of_sensors_bytes += HEADER_SIZE + member_size(sensor_data, soil_moist_1);
  if(sc->soil_moist_2)
    sc->number_of_sensors_bytes += HEADER_SIZE + member_size(sensor_data, soil_moist_2);
  if(sc->lum)
    sc->number_of_sensors_bytes += HEADER_SIZE + member_size(sensor_data, lum);

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
    switch (sc->air_sensor)
    {
      case IC_BME280:
        Serial.println(F("BME280 init..."));
        Wire.begin();
        bme280.setI2CAddress(0x76);
        if(bme280.beginI2C()) 
          Serial.println("OK!\n");
        else
          Serial.println("FALSE!\n");
        break;
      case IC_BME680:
        Serial.print(F("BME680 init..."));
        if(BME680.begin(I2C_STANDARD_MODE)) 
        {
          BME680.setOversampling(TemperatureSensor, Oversample16);
          BME680.setOversampling(HumiditySensor, Oversample16);
          BME680.setOversampling(PressureSensor, Oversample16);
          BME680.setIIRFilter(IIR4);
          BME680.setGas(320, 150);  // 320c for 150 milliseconds
          Serial.print(F("OK!\r\n"));

          static int32_t  temp, humidity, pressure, gas;
          BME680.getSensorData(temp, humidity, pressure, gas);  // Initial readout
        }
        else
        {
          Serial.print(F("FAIL...\r\n"));
        }
        break;
      default:
        Serial.println("AIR SENSOR not present!");
    }
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
  switch (sc->air_sensor)
  {
    case IC_BME280:
      if(sc->air_temp)
        sd->air_temp = bme280.readTempC() * 100;
      if(sc->air_hum)
        sd->air_hum = bme280.readFloatHumidity() * 100;
      if(sc->air_pres)  
        sd->air_pres = bme280.readFloatPressure();
      break;
    case IC_BME680:
      int32_t  temp, humidity, pressure, gas;  // BME readings
      BME680.getSensorData(temp, humidity, pressure, gas);  // Get readings

      if(sc->air_temp)
        sd->air_temp = ((float)temp);
      if(sc->air_hum)
        sd->air_hum = ((float)humidity) / 10;
      if(sc->air_pres)  
        sd->air_pres = ((float)pressure);
      break;
    default:
      break;
  }

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
    if((*size) + HEADER_SIZE + member_size(sensor_data, air_temp) < length)
    {
      data[(*size)++] = (uint8_t)AIR_TEMPERATURE;
      memcpy(&data[*size], (uint8_t *)&sd->air_temp, 2);
      *size += member_size(sensor_data, air_temp);
    }
    else
      return false;
  }

  if(sc->air_hum)
  {
    if((*size) + HEADER_SIZE + member_size(sensor_data, air_hum) < length)
    {
      data[(*size)++] = (uint8_t)AIR_HUMIDITY;
      memcpy(&data[*size], (uint8_t *)&sd->air_hum, 2);
      *size += member_size(sensor_data, air_hum);
    }
    else
      return false;
  }

  if(sc->air_pres)
  {
    if((*size) + HEADER_SIZE + member_size(sensor_data, air_pres) < length)
    {
      data[(*size)++] = (uint8_t)AIR_PRESSURE;
      memcpy(&data[*size], (uint8_t *)&sd->air_pres, 4);
      *size += member_size(sensor_data, air_pres);
    }
    else
      return false;
  }

  if(sc->soil_temp_1)
  {
    if((*size) + HEADER_SIZE + member_size(sensor_data, soil_temp_1) < length)
    {
      data[(*size)++] = (uint8_t)SOIL_TEMPERATURE_1;
      memcpy(&data[*size], (uint8_t *)&sd->soil_temp_1, 2);
      *size += member_size(sensor_data, soil_temp_1);
    }
    else
      return false;
  }

  if(sc->soil_temp_2)
  {
    if((*size) + HEADER_SIZE + member_size(sensor_data, soil_temp_2) < length)
    {
      data[(*size)++] = (uint8_t)SOIL_TEMPERATURE_2;
      memcpy(&data[*size], (uint8_t *)&sd->soil_temp_2, 2);
      *size += member_size(sensor_data, soil_temp_2);
    }
    else
      return false;
  }

  if(sc->soil_moist_1)
  {
    if((*size) + HEADER_SIZE + member_size(sensor_data, soil_moist_1) < length)
    {
      data[(*size)++] = (uint8_t)SOIL_MOISTURE_1;
      memcpy(&data[*size], (uint8_t *)&sd->soil_moist_1, 1);
      *size += member_size(sensor_data, soil_moist_1);
    }
    else
      return false;
  }

  if(sc->soil_moist_2)
  {
    if((*size) + HEADER_SIZE + member_size(sensor_data, soil_moist_2) < length)
    {
      data[(*size)++] = (uint8_t)SOIL_MOISTURE_2;
      memcpy(&data[*size], (uint8_t *)&sd->soil_moist_2, 1);
      *size += member_size(sensor_data, soil_moist_2);
    }
    else
      return false;
  }

  if(sc->lum)
  {
    if((*size) + HEADER_SIZE + member_size(sensor_data, lum) < length)
    {
      data[(*size)++] = (uint8_t)LUMINOSITY;
      memcpy(&data[*size], (uint8_t *)&sd->lum, 2);
      *size += member_size(sensor_data, lum);
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
      if(!sc->air_temp && (HEADER_SIZE + member_size(sensor_data, air_temp) <= size))
      {
        sd->air_temp = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + member_size(sensor_data, air_temp);
        return true;
      }
      else
        return false;
    case AIR_HUMIDITY:
      if(!sc->air_hum && (HEADER_SIZE + member_size(sensor_data, air_hum) <= size))
      {
        sc->air_hum = true;
        sd->air_hum = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + member_size(sensor_data, air_hum);
        return true;
      }
      else
        return false;
    case AIR_PRESSURE:
      if(!sc->air_pres && (HEADER_SIZE + member_size(sensor_data, air_pres) <= size))
      {
        sc->air_pres = true;
        sd->air_pres = ((uint32_t)data[4] << 24) | ((uint32_t)data[3] << 16) | ((uint32_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + member_size(sensor_data, air_pres);
        return true;
      }
      else
        return false;
    case SOIL_TEMPERATURE_1:
      if(!sc->soil_temp_1 && (HEADER_SIZE + member_size(sensor_data, soil_temp_1) <= size))
      {
        sc->soil_temp_1 = true;
        sd->soil_temp_1 = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + member_size(sensor_data, soil_temp_1);
        return true;
      }
      else
        return false;
    case SOIL_TEMPERATURE_2:
      if(!sc->soil_temp_2 && (HEADER_SIZE + member_size(sensor_data, soil_temp_2) <= size))
      {
        sc->soil_temp_2 = true;
        sd->soil_temp_2 = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + member_size(sensor_data, soil_temp_2);
        return true;
      }
      else
        return false;
    case SOIL_MOISTURE_1:
      if(!sc->soil_moist_1 && (HEADER_SIZE + member_size(sensor_data, soil_temp_1) <= size))
      {
        sc->soil_moist_1 = true;
        sd->soil_moist_1 = data[1];
        *position += HEADER_SIZE + member_size(sensor_data, soil_temp_1);
        return true;
      }
      else
        return false;
    case SOIL_MOISTURE_2:
      if(!sc->soil_moist_2 && (HEADER_SIZE + member_size(sensor_data, soil_moist_2) <= size))
      {
        sc->soil_moist_2 = true;
        sd->soil_moist_2 = data[1];
        *position += HEADER_SIZE + member_size(sensor_data, soil_moist_2);
        return true;
      }
      else
        return false;
    case LUMINOSITY:
      if(!sc->lum && (HEADER_SIZE + member_size(sensor_data, lum) <= size))
      {
        sc->lum = true;
        sd->lum = ((uint16_t)data[2] << 8) | data[1];
        *position += HEADER_SIZE + member_size(sensor_data, lum);
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
    if(!parseSensorValue(sd, sc, &data[position], size - position, &position))
      return false;

  sc->number_of_sensors_bytes = position;

  return true;
}

void printSensorData(sensor_data *sd, sensors_config *sc)
{  
  if(sc->lum)
  {
    Serial.println("*** Luminosity ***");
    Serial.println("\tL = " + (String)(sd->lum) + " lux");
  }
  
  if(sc->soil_temp_1 || sc->soil_temp_2)
  {
    Serial.println("*** Soil temperature ***");
    if(sc->soil_temp_1)
      Serial.println("\tT1 = " + (String)(sd->soil_temp_1 / 100) + "." + (String)(sd->soil_temp_1 % 100) + " \xC2\xB0\x43");
    if(sc->soil_temp_2)
      Serial.println("\tT2 = " + (String)(sd->soil_temp_2 / 100) + "." + (String)(sd->soil_temp_2 % 100) + " \xC2\xB0\x43");
  }

  if(sc->soil_moist_1 || sc->soil_moist_2)
  {
    Serial.println("*** Soil moisture ***");
    if(sc->soil_moist_1)
      Serial.println("\tH1 = " + (String)sd->soil_moist_1 + " %");
    if(sc->soil_moist_2)
      Serial.println("\tH2 = " + (String)sd->soil_moist_2 + " %");
  }

  if(sc->air_hum || sc->air_temp || sc->air_pres)
  {
    Serial.println("*** Air parameters ***");
    if(sc->air_sensor == IC_BME280)
      Serial.println("** BME280 **");
    else if(sc->air_sensor == IC_BME680)
      Serial.println("** BME680 **");

    if(sc->air_hum)
      Serial.println("\tH = " + String(sd->air_hum / 100) + "." + (String)(sd->air_hum % 100) + " \%");
    if(sc->air_temp)
      Serial.println("\tT = " + String(sd->air_temp / 100) + "." + (String)(sd->air_temp % 100) + " \xC2\xB0\x43");
    if(sc->air_pres)
      Serial.println("\tP = " + String(sd->air_pres / 100) + "." + (String)(sd->air_pres % 100) + " mBar");
  }
}