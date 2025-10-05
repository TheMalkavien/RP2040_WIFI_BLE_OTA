#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <NimBLEDevice.h>
#include "uploader.h"
#include "config.h"
#include "main.h"
#include "rp2040_flasher.h"

#define FW_SERVICE_UUID   "d3a8f820-9b39-4a7c-9d09-7b5e5a313001"
#define CTRL_CHAR_UUID    "d3a8f821-9b39-4a7c-9d09-7b5e5a313001"
#define DATA_CHAR_UUID    "d3a8f822-9b39-4a7c-9d09-7b5e5a313001"
#define NOTIF_CHAR_UUID   "d3a8f823-9b39-4a7c-9d09-7b5e5a313001"

class BleUpload : public Uploader {
  public:
    BleUpload();
    ~BleUpload() = default;
    void Setup() override;
    void notifyClients(const String &message) override;
    void loop() override;
    bool hasClient() const { return clientConnected; }
    void setClientConnected(bool v) { clientConnected = v; }

    void handleCtrlCommand(const std::string& s);
    void onDataChunk(const uint8_t* data, size_t len);

  private:
    bool clientConnected = false;
    unsigned long lastAdvToggle = 0;
    bool advRunning = false;
    NimBLEAdvertising* adv = nullptr;
    NimBLEServer* server = nullptr;
    NimBLEService* service = nullptr;
    NimBLECharacteristic* ctrlChar = nullptr;
    NimBLECharacteristic* dataChar = nullptr;
    NimBLECharacteristic* notifChar = nullptr;

    File binFile;
    size_t expectedSize = 0;
    size_t received = 0;
    int lastProgressPct = -1; 

    void beginUpload(size_t total);
    void endUpload();
};
