#ifndef _BLE_CLIENT_H
#define _BLE_CLIENT_H

#include "sensors.h"

void BLE_debugEnable(bool enable);

void BLE_clientSetup(char *serv_uuid, char *char_uuid);

bool BLE_connectToServer();
bool BLE_disconnectFromServer();
bool BLE_send(uint8_t packet[], uint16_t size);
bool BLE_recv(uint8_t *rx_buffer, uint16_t *rx_buffer_len);

void BLE_getServerMac(uint8_t *server_mac);
void BLE_getMACStandalone(uint8_t *data);

void BLE_pullSensorDataArray(uint8_t *sensor_data, uint16_t *sensor_data_len);

#endif
