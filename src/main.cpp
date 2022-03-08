#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include <RGB_LED.h>
#include <RS485.h>
#include <WiFi_client.h>
#include <BLE_server.h>
#include <file_utils.h>
#include <crypto_utils.h>
#include "sensors.h"
#include "sdu.h"
#include "ldu.h"
#include "json.h"
#include <mbedtls/md.h>

json_config jc;

uint8_t packet[256];
uint16_t packet_len;
uint8_t udp_packet[6 + sizeof(sensor_data)];

uint8_t sensor_data_packet[256];
uint16_t sensor_data_packet_length;

uint8_t gateaway_mac[6];

SDU_struct comm_params;
char topic_to_subscribe[64];

LDU_struct loc_comm_params;
uint8_t devices_hash[32];


void GetMacAddress()
{
  //https://techtutorialsx.com/2018/03/09/esp32-arduino-getting-the-bluetooth-device-address/
  btStart();
  esp_bluedroid_init();
  esp_bluedroid_enable();

  Serial.print("BLE MAC Address:  ");
  const uint8_t* point = esp_bt_dev_get_address();
  for (int i = 0; i < 6; i++)
  { 
    char str[3];
   
    sprintf(str, "%02X", (int)point[i]);
    Serial.print(str);
    if (i < 5)
      Serial.print(":");
    udp_packet[i] = point[i];
  }
  Serial.println();
  
  String mac = WiFi.macAddress();
  
  Serial.print("WiFi MAC Address: ");
  Serial.println(mac);

  for (int i = 0; i < 6; i++)
  {
    uint8_t hi, lo;
    char c = mac.charAt(3 * i); 
    hi = c > '9' ? c - 'A' + 10 : c - '0';
    c = mac.charAt(3 * i + 1);
    lo = c > '9' ? c - 'A' + 10 : c - '0';
  }
}

void WiFi_loop()
{
  //LED_setSaturation(32);
  while (1)
  {
    RGB_LED_setColor(BLUE);

    SDU_debugEnable(true);

    memset(&packet[6], 0x00, sizeof(packet)-6);

    sensor_data sd;
    getSensorData(&sd, &jc.sc);
    printSensorData(&sd, &jc.sc);

    if(!convertToSensorDataArray(&packet[7], 256-7, &packet_len, &sd, &jc.sc))
      Serial.println("Conversion failed");
    
    packet[6] = packet_len;
    packet_len += 7;

    uint8_t ret = SDU_sendData(&comm_params, packet, packet_len);
    SDU_debugPrintError(ret);

    RGB_LED_setColor(BLACK);

    delay(5*60*1000);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("--- IoTartic Sensing Unit ---");  
    
  RGB_LED_init();
  RGB_LED_setSaturation(255);

  FS_setup();
  
  char json[2048];
  FS_readFile(SPIFFS, "/config.json", json);

  DynamicJsonDocument config(2048);
  deserializeJson(config, json);

  while(!getJsonConfig(&jc, &config))
  {
    Serial.println("Json config error!");
    delay(1000);
  }

  while(jc.device_type != SENSOR)
  {
    Serial.println("Device not configured as sensor type!");
    delay(1000);
  }

  Serial.print("WiFi ssid: ");
  Serial.println(jc.wifi_ssid);
  Serial.print("WiFi pass: ");
  Serial.println(jc.wifi_pass);
  
  initSensors(&jc.sc);
  //while(1);

  if (jc.standalone)
  {
    if (jc.server_tunnel == WIFI)
    {
      Serial.println("WiFi server communication");

      uint8_t ret;
      
      WiFI_debugEnable(true);
      SDU_debugEnable(true);

      BLE_getMACStandalone(gateaway_mac);

      SDU_init(&comm_params, jc.comm_mode, jc.protocol, jc.server_tunnel, (char *) jc.ip, jc.port, (char *) jc.server_salt, (char *)jc.server_password, (uint8_t*) gateaway_mac);
      SDU_setWIFIparams(&comm_params, jc.wifi_ssid, jc.wifi_pass);

      if (jc.protocol == MQTT)
      {
        memcpy(topic_to_subscribe, jc.subscribe_topic, strlen(jc.subscribe_topic));
        for(uint8_t i= 0; i < 6; i++)
            sprintf(topic_to_subscribe + strlen(jc.subscribe_topic) + 2*i, "%02x", (int)gateaway_mac[i]);
        topic_to_subscribe[strlen(jc.subscribe_topic) + 12] = 0;
        SDU_setMQTTparams(&comm_params, jc.client_id, jc.publish_topic, topic_to_subscribe);
      }

      if (jc.comm_mode == ENCRYPTED_COMM)
      {
        ret = SDU_updateIV(&comm_params);
        SDU_debugPrintError(ret);

        ret = SDU_handshake(&comm_params);
        SDU_debugPrintError(ret);
      }

      memcpy(packet, gateaway_mac, 6);

      WiFi_loop();
    } 
  }
  else
  {   
    LDU_debugEnable(true);
    LDU_setBLEParams(&loc_comm_params, jc.serv_uuid, jc.char_uuid, jc.ble_password); 
    if (jc.local_tunnel == BLE)
    {
      Serial.println("BLE local communication");
      BLE_getMACStandalone(gateaway_mac);
    }
    else if (jc.local_tunnel == RS485)
    {
      Serial.println("RS485 local communication");
      
      LDU_setRS485Params(&loc_comm_params, (uint32_t)115200);

      BLE_getMACStandalone(gateaway_mac);
      memcpy(sensor_data_packet, gateaway_mac, 6);

    }
    LDU_init(&loc_comm_params);
  }
}

void loop()
{
  memset(packet, 0, sizeof(packet));
  packet_len = sizeof(packet);
  LDU_recv(&loc_comm_params, (char *)packet, &packet_len, (uint32_t)5000);

  String packet_s = String((char *)packet);
  Serial.println(packet_s);

  uint16_t header;
  if(packet_len != 0)
  {
    Serial.println("LEN OK");
    if(LDU_parsePacket(&loc_comm_params, packet, packet_len, &header) == LDU_OK)
    {
      Serial.println("Parse OK");
      if (header == SENSOR_MAC_ADDRESS_REQUEST_HEADER)
      {
        Serial.println("MAC REQ");
        LDU_sendBLEMAC(&loc_comm_params, gateaway_mac);
      }
      else if (header == SENSOR_DATA_REQUEST_HEADER)
      {
        Serial.println("DATA REQ");
        sensor_data sd;
      
        getSensorData(&sd, &jc.sc);
        printSensorData(&sd, &jc.sc);

        sensor_data_packet[0] = jc.sc.number_of_sensors_bytes;

        convertToSensorDataArray(&sensor_data_packet[1], 255, &sensor_data_packet_length, &sd, &jc.sc);
        sensor_data_packet_length += 1;

        LDU_sendSensorData(&loc_comm_params, sensor_data_packet, sensor_data_packet_length);
      }
    }
  }
}
