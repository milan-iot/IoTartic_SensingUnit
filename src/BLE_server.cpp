/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
    https://github.com/nkolban/ESP32_BLE_Arduino
*/

#include <Arduino.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>

#include "BLE_server.h"
#include "LED.h"
#include "file_utils.h"

BLECharacteristic *pCharacteristic;
bool data_received_flag = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

class MyServerCallbacks: public BLEServerCallbacks 
{
    void onConnect(BLEServer* pServer) 
    {
      Serial.println("BLE Connected");
      LED_setColor(BLUE);
      delay(1000);
      LED_setColor(BLACK);
    };

    void onDisconnect(BLEServer* pServer) 
    {
      Serial.println("BLE Disconnected");
      BLEDevice::startAdvertising();
      LED_setColor(PURPLE);
      delay(200);
      LED_setColor(BLACK);
    }
};

class MyCallbacks: public BLECharacteristicCallbacks 
{
  void onRead(BLECharacteristic *pCharacteristic) 
  {
    Serial.println("BLE Read");
    LED_setColor(GREEN);
    delay(200);
    LED_setColor(BLACK);
  }
  
  void onWrite(BLECharacteristic *pCharacteristic) 
  {
    Serial.println("BLE Write");
    data_received_flag = true;
    LED_setColor(RED);
    delay(200);
    LED_setColor(BLACK);
  }
};

bool BLE_serverSetup(char *serv_uuid, char *char_uuid)
{
  Serial.println("Starting BLE work!");
  BLEDevice::init("IO3T-SU");

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();

  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(serv_uuid);

  pCharacteristic = pService->createCharacteristic(
                                        char_uuid,
                                        BLECharacteristic::PROPERTY_READ |
                                        BLECharacteristic::PROPERTY_WRITE
                                      );

  pCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(serv_uuid);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("Characteristic defined!");
  return true;
}

bool BLE_available()
{
  return data_received_flag;
}

void BLE_send(uint8_t *data, uint16_t data_length)
{
  pCharacteristic->setValue(data, data_length);

}

bool BLE_recv(char *data, uint16_t *size, uint32_t timeout)
{
  uint32_t t0 = millis();
  while (!data_received_flag && (millis() - t0) < timeout);
  delay(100);

  if(data_received_flag)
  {
    String rxData = pCharacteristic->getValue().c_str();
   
    Serial.println(*size);
    rxData.toCharArray(data, *size);

    //if(*size > rxData.length())
      *size = rxData.length();
    data[*size] = 0;
    
    data_received_flag = false;
  }
  else
  {
    *size = 0;
    return false;
  }

  return true;
}

void BLE_getMACStandalone(uint8_t *data)
{
  btStart();
  esp_bluedroid_init();
  esp_bluedroid_enable();
  const uint8_t* gateaway_mac = esp_bt_dev_get_address();
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  btStop();

  memcpy(data, gateaway_mac, 6);
}