#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include <RGB_LED.h>
#include <RS485.h>
#include <WiFi_client.h>
#include <BLE_client.h>
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


#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10         /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

void WiFi_loop()
{
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

    WiFi_disconnect();

    RGB_LED_setColor(BLACK);

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
    " Seconds");

    Serial.println("Going to sleep now");
    Serial.flush(); 
    esp_deep_sleep_start();
    Serial.println("This will never be printed");
  }
}

void setup()
{
  delay(5000);

  Serial.begin(115200);
  Serial.println("--- IoTartic Sensing Unit ---");  

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

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

  initSensors(&jc.sc);

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
      memcpy(sensor_data_packet, gateaway_mac, 6);
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
  LDU_debugEnable(true);
  BLE_debugEnable(false);
}

void loop()
{
  RGB_LED_setColor(BLUE);

  memset(&sensor_data_packet[6], 0x00, sizeof(sensor_data_packet)-6);

  sensor_data sd;
  getSensorData(&sd, &jc.sc);
  printSensorData(&sd, &jc.sc);

  if(!convertToSensorDataArray(&sensor_data_packet[7], 256-7, &sensor_data_packet_length, &sd, &jc.sc))
    Serial.println("Conversion failed");
  
  sensor_data_packet[6] = sensor_data_packet_length;
  sensor_data_packet_length += 7;

  Serial.print("Packet len: ");
  Serial.println(sensor_data_packet_length);

  uint8_t ret = LDU_sendSensorData(&loc_comm_params, sensor_data_packet, sensor_data_packet_length);

  packet_len = 0;
  LDU_recv(&loc_comm_params, (char *)packet, &packet_len, (uint32_t)5000);

  uint16_t header;
  if(LDU_parsePacket(&loc_comm_params, packet, packet_len, &header) != LDU_OK)
    Serial.println("PARSE ERROR");

  RGB_LED_setColor(BLACK);
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  " Seconds");

  Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}