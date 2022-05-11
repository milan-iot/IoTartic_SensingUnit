#include <Arduino.h>
#include <ArduinoJson.h>

#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "BLEDevice.h"
#include "file_utils.h"
#include "BLE_client.h"

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;
static char SERV_UUID[64];
static char CHAR_UUID[64];
static char ble_server_addr[6];

static BLEClient* pClient;

bool BLE_client_debug_enable = false;

void BLE_debugEnable(bool enable)
{
  BLE_client_debug_enable = enable;
}

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData,
                           size_t length,
                           bool isNotify) 
{
  if (BLE_client_debug_enable)
    Serial.println("Notify callbeck.");
}

class MyClientCallback : public BLEClientCallbacks 
{
  void onConnect(BLEClient* pclient)
  {
    if (BLE_client_debug_enable)
      Serial.println("onConnect callbeck.");
  }

  void onDisconnect(BLEClient* pclient) 
  {
    connected = false;
    doConnect = true;
    if (BLE_client_debug_enable)
      Serial.println("onDisconnect callbeck.");
  }
};

bool connectToServer() 
{
  BLEUUID serviceUUID(SERV_UUID);
  BLEUUID charUUID(CHAR_UUID);

  if (BLE_client_debug_enable)
  {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
  }
  
  pClient  = BLEDevice::createClient();
  if (BLE_client_debug_enable)
    Serial.println(" - Created client");
  
  pClient->setClientCallbacks(new MyClientCallback());
  
  // Connect to the remote BLE Server.
  if (!pClient->connect(myDevice))
  {
    if (BLE_client_debug_enable)
      Serial.println(" - Failed to connect to server");
    return false;
  }
  if (BLE_client_debug_enable)
    Serial.println(" - Connected to server");
  
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) 
  {
    if (BLE_client_debug_enable)
    {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
    }
    pClient->disconnect();
    return false;
  }
  if (BLE_client_debug_enable)
    Serial.println(" - Found our service");
  
  
  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) 
  {
    if (BLE_client_debug_enable)
    {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
    }
    pClient->disconnect();
    return false;
  }
  if (BLE_client_debug_enable)
    Serial.println(" - Found our characteristic");
  
  // assign for notify
  //pRemoteCharacteristic->registerForNotify(notifyCallback);

  connected = true;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks 
{
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) 
  {
    BLEUUID serviceUUID(SERV_UUID);
    BLEUUID charUUID(CHAR_UUID);
    
    if (BLE_client_debug_enable)
    {
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());
    }

    uint8_t* tmp = (uint8_t *)advertisedDevice.getAddress().getNative();
    for (int i = 0; i < 6; i++)
      ble_server_addr[i] = tmp[i];

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks


void BLE_clientSetup(char *serv_uuid, char *char_uuid) 
{ 
  memcpy(SERV_UUID, serv_uuid, strlen(serv_uuid));
  memcpy(CHAR_UUID, char_uuid, strlen(char_uuid));

  if (BLE_client_debug_enable)
    Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("IO3T-MU");

  if (BLEDevice::setMTU(100) != ESP_OK)
  {
    if (BLE_client_debug_enable)
    {
      Serial.println("BLE setMTU failed.");
    }
    return;
  }

  if (BLE_client_debug_enable)
  {
    Serial.print("getMTU(): ");
    Serial.println(BLEDevice::getMTU(), HEX);
  }

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
} // End of setup.


bool BLE_connectToServer()
{
  if (doConnect == true) 
  {
    if (connectToServer())
    {
      if (BLE_client_debug_enable)
        Serial.println("We are now connected to the BLE Server."); 
      doConnect = false;
    }
    else 
    {
      if (BLE_client_debug_enable)
        Serial.println("We have failed to connect to the server; there is nothin more we will do.");
      doConnect = false; // Zasto ne radi sa true?
    }
  }
  return !doConnect;
}

bool BLE_disconnectFromServer()
{
  pClient->disconnect();
  return true;
}

bool BLE_send(uint8_t packet[], uint16_t size)
{
  if (connected)
  { 
    pRemoteCharacteristic->writeValue(packet, size, true);
    return true;
  }
  return false;
}

bool BLE_recv(uint8_t *rx_buffer, uint16_t *rx_buffer_len)
{
  if (connected)
  {
      std::string rxValue = pRemoteCharacteristic->readValue();

      *rx_buffer_len = rxValue.length();
      uint8_t *ptr = rx_buffer;
      for (uint8_t i = 0; i < *rx_buffer_len; i++)
      {
        *ptr = rxValue[i];
        ptr++;
      }
      //rx_buffer = (uint8_t *)(&rxValue[0]);

      return true;
  }
  return false;
}

void BLE_getServerMac(uint8_t *server_mac)
{
  for (int i = 0; i < 6; i++)
    server_mac[i] = ble_server_addr[i];
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



/*******************************************************************************************************************************/
// deprecated
void BLE_pullSensorDataArray(uint8_t *sensor_data, uint16_t *sensor_data_len)//sensor_data_hashed *sdh) 
{
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  Serial.println("get test");
  if (doConnect == true) 
  {
    if (connectToServer()) 
      Serial.println("We are now connected to the BLE Server."); 
    else 
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) 
  {
    String newValue = "READ";

    Serial.print("getMTU(): ");
    Serial.println(BLEDevice::getMTU(), HEX);
    
    // Set the characteristic's value to be the array of bytes that is actually a string.
    pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());

    std::string rxValue = pRemoteCharacteristic->readValue();

    //Serial.println(sizeof(sensor_data_hashed));
    Serial.println(rxValue.length());
    *sensor_data_len = rxValue.length();
    
    Serial.println("-----------------------------");

    uint8_t *ptr = sensor_data;
    for (uint8_t i = 0; i < rxValue.length(); i++)
    {
      *ptr = rxValue[i];
      ptr++;
    }
    sensor_data = (uint8_t *)(&rxValue[0]);
    
  }
  else if(doScan)
    BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino   
}