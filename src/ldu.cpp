#include "ldu.h"


bool LDU_debug_enable = false;

void LDU_debugEnable(bool enable)
{
    LDU_debug_enable = enable;
}

void LDU_debugPrint(int8_t *title, uint8_t *data, uint16_t data_len)
{
    if(LDU_debug_enable)
        Crypto_debugPrint(title, data, data_len);
}

uint8_t LDU_constructPacket(char *password,  uint16_t header_type, uint8_t *in_data, uint8_t in_data_len, uint8_t *out_data, uint16_t *out_data_len)
{
    CRC32 crc;
    crc.setPolynome(CRC32_DEFAULT_VALUE);

    // copy header
    out_data[0] = header_type >> 8;
    out_data[1] = header_type & 0xff;

    switch(header_type)
    {
        case SENSOR_MAC_ADDRESS_VALUE_HEADER:
            if (in_data_len != SENSOR_MAC_ADDRESS_LENGTH)
                return LOCAL_ERROR(INVALID_NUM_OF_BYTES);
            memcpy(out_data + HEADER_LENGTH, in_data, in_data_len);
            *out_data_len = HEADER_LENGTH + in_data_len;
        break;

        case SENSOR_DATA_VALUE_HEADER:
            if (in_data_len < 1)
                return LOCAL_ERROR(INVALID_NUM_OF_BYTES);
            memcpy(out_data + HEADER_LENGTH, in_data, in_data_len);
            *out_data_len = HEADER_LENGTH + in_data_len;
        break;

        default:
            return LOCAL_ERROR(INVALID_HEADER);
        break;
    }

    uint8_t hmac_value[32];
    mbedtls_md_context_t ctx;
    Crypto_Digest(&ctx, HMAC_SHA256, (uint8_t *)&out_data[HEADER_LENGTH], *out_data_len - HEADER_LENGTH, hmac_value, (uint8_t *) password, strlen(password));

    crc.add(hmac_value, sizeof(hmac_value));
    uint32_t crc32 = crc.getCRC();

    memcpy(out_data + *out_data_len, (uint8_t *)&crc32, CRC32_LENGTH);
    *out_data_len += CRC32_LENGTH;

    if (LDU_debug_enable)
    {
        Serial.print("CRC: ");
        Serial.println(crc32, HEX);
    }

    return LDU_OK;
}

bool LDU_checkDevicesHash(uint8_t *devices_hash, uint8_t *recieved_hash)
{
    for(uint8_t i = 0; i < HASH_LENGTH; i++)
        if(devices_hash[i] != recieved_hash[i])
            return false;

    return true;
}

uint8_t LDU_parsePacket(LDU_struct *comm_params, uint8_t *input, uint16_t input_length, uint16_t *header)
{
    switch(comm_params->mode)
    {
        case BLE:
            if(input_length < HEADER_LENGTH)
                return LOCAL_ERROR(INVALID_NUM_OF_BYTES);

            *header = ((uint16_t) input[0] << 8) | input[1];

            if(input[HEADER_LENGTH] != SUCCESS)
                return LOCAL_ERROR(DATA_TRANSFER_ERROR);
        break;

        case RS485:
            if(input_length < HASH_LENGTH + HEADER_LENGTH)
                return LOCAL_ERROR(INVALID_NUM_OF_BYTES);

            if(!LDU_checkDevicesHash(comm_params->devices_hmac, input))
                return INVALID_AUTH;
            
            *header = ((uint16_t) input[0 + HASH_LENGTH] << 8) | input[1 + HASH_LENGTH];
        break;

        default:
            return BAD_COM_STRUCTURE;
    }

    return LDU_OK;
}

bool LDU_calculateDevicesHash(LDU_struct *comm_params)
{
    uint8_t hash_input[80];
    uint8_t hash_input_length = 0;
    mbedtls_md_context_t ctx;

    memcpy(hash_input, comm_params->serv_uuid, strlen(comm_params->serv_uuid));
    hash_input_length += strlen(comm_params->serv_uuid);

    memcpy(hash_input, comm_params->char_uuid, strlen(comm_params->char_uuid));
    hash_input_length += strlen(comm_params->char_uuid);

    return Crypto_Digest(&ctx, HMAC_SHA256, hash_input, hash_input_length, comm_params->devices_hmac, (uint8_t *)comm_params->ble_password, strlen(comm_params->ble_password));
}

void LDU_setBLEParams(LDU_struct *comm_params, char *serv_uuid, char *char_uuid, char *ble_password)
{
    comm_params->mode = BLE;
    comm_params->serv_uuid = serv_uuid;
    comm_params->char_uuid = char_uuid;
    comm_params->ble_password = ble_password;

    LDU_calculateDevicesHash(comm_params);
}

void LDU_setRS485Params(LDU_struct *comm_params, uint32_t rs485_baudrate)
{
    comm_params->mode = RS485;
    comm_params->rs485_baudrate = rs485_baudrate;
}

uint8_t LDU_init(LDU_struct *comm_params)
{
    switch(comm_params->mode)
    {
        case BLE:
            BLE_clientSetup(comm_params->serv_uuid, comm_params->char_uuid);
        break;

        case RS485:
            RS485_begin(comm_params->rs485_baudrate);
            RS485_setMode(RS485_RX);
        break;

        default:
            return BAD_COM_STRUCTURE;
    }
    return LDU_OK;
}

uint8_t LDU_send(LDU_struct *comm_params, uint8_t packet[], uint16_t size)
{
    switch(comm_params->mode)
    {
        case BLE:
            BLE_connectToServer();
            BLE_send(packet, size);
        break;

        case RS485:
            RS485_setMode(RS485_TX);
            RS485_send(packet, size);
            RS485_setMode(RS485_RX);
        break;

        default:
            return BAD_COM_STRUCTURE;
    }

    LDU_debugPrint((int8_t *)"LDU send: ", packet, size);
    return LDU_OK;
}

uint8_t LDU_recv(LDU_struct *comm_params, char rx_buffer[], uint16_t *size, uint32_t timeout = 5000)
{
    switch(comm_params->mode)
    {
        case BLE:
            BLE_recv((uint8_t *)rx_buffer, size);
            BLE_disconnectFromServer();
        break;
        
        case RS485:
            RS485_recv((uint8_t *)rx_buffer, size, timeout);
        break;
        
        default:
            return BAD_COM_STRUCTURE;
    }
    
    LDU_debugPrint((int8_t *)"LDU recieve: ", (uint8_t *)rx_buffer, *size);
    return LDU_OK;
}

uint8_t LDU_sendPacket(LDU_struct *comm_params, uint16_t header, uint8_t *data, uint16_t data_length)
{
    uint8_t packet[256];
    uint16_t packet_length = 0;

    LDU_constructPacket(comm_params->ble_password, 
                        header,
                        data,
                        data_length,
                        packet,
                        &packet_length
                        );

    return LDU_send(comm_params, packet, packet_length);
}

uint8_t LDU_sendBLEMAC(LDU_struct *comm_params, uint8_t ble_mac[])
{
    return LDU_sendPacket(comm_params, SENSOR_MAC_ADDRESS_VALUE_HEADER, ble_mac, SENSOR_MAC_ADDRESS_LENGTH);
}

uint8_t LDU_sendSensorData(LDU_struct *comm_params, uint8_t data[], uint16_t data_length)
{
    return LDU_sendPacket(comm_params, SENSOR_DATA_VALUE_HEADER, data, data_length);
}