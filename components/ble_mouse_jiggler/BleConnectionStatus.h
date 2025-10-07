#ifndef BLE_CONNECTION_STATUS_H
#define BLE_CONNECTION_STATUS_H

#include "BLEServer.h"
#include "BLECharacteristic.h"

class BleConnectionStatus : public BLEServerCallbacks
{
public:
    BleConnectionStatus(void);
    bool connected = false;
    void onConnect(BLEServer *pServer);
    void onDisconnect(BLEServer *pServer);
    BLECharacteristic* inputMouse;
};

#endif // BLE_CONNECTION_STATUS_H
