#include "ble_upload.h"
#include "config.h"
#include "rp2040_flasher.h"
#include "main.h"

extern Uploader* uploader;

static BleUpload* gBle = nullptr;

// ===== Callbacks compatibles NimBLE-Arduino =====
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s) override {
    if (gBle) gBle->notifyClients("log:Client BLE connecté.");
  }
  void onDisconnect(NimBLEServer* s) override {
    if (gBle) gBle->notifyClients("error:Client BLE déconnecté.");
    s->getAdvertising()->start();
  }
};

class CtrlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    if (!gBle) return;
    std::string v = c->getValue();
    gBle->handleCtrlCommand(v);
  }
};

class DataCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    if (!gBle) return;
    std::string v = c->getValue();
    if (!v.empty()) {
      gBle->onDataChunk(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    }
  }
};

BleUpload::BleUpload() { gBle = this; }

void BleUpload::Setup() {
  NimBLEDevice::init(MY_BLE_NAME);

  #if defined(ESP_PWR_LVL_P9)
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  #elif defined(ESP_PWR_LVL_P7)
    NimBLEDevice::setPower(ESP_PWR_LVL_P7);
  #elif defined(ESP_PWR_LVL_P3)
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
  #else
    NimBLEDevice::setPower(ESP_PWR_LVL_N0);
  #endif

  NimBLEDevice::setMTU(517);

  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  service = server->createService(FW_SERVICE_UUID);

  ctrlChar = service->createCharacteristic(
      CTRL_CHAR_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  ctrlChar->setCallbacks(new CtrlCallbacks());

  dataChar = service->createCharacteristic(
      DATA_CHAR_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  dataChar->setCallbacks(new DataCallbacks());

  notifChar = service->createCharacteristic(
      NOTIF_CHAR_UUID,
      NIMBLE_PROPERTY::NOTIFY
  );

  service->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(FW_SERVICE_UUID);
  adv->setScanResponse(true);
  adv->start();

  notifyClients("log:BLE prêt. Publicité en cours.");
}


void BleUpload::notifyClients(const String &message) {
  if (!notifChar) return;
  notifChar->setValue((uint8_t*)message.c_str(), message.length());
  notifChar->notify(true);
}

void BleUpload::loop() {
  // Rien à faire ici
}

void BleUpload::beginUpload(size_t total) {
  resetInactivityTimer();
  if (binFile) binFile.close();
  expectedSize = total;
  received = 0;
  lastProgressPct = -1;
  binFile = LittleFS.open("/firmware.bin", "w");
  if (!binFile) {
    notifyClients("error:Impossible d'ouvrir /firmware.bin");
    return;
  }
  notifyClients("log:Début du téléversement BLE...");
}

void BleUpload::endUpload() {
  resetInactivityTimer();
  if (binFile) {
    binFile.close();
    lastProgressPct = -1;
    notifyClients("EVENT:UPLOAD_COMPLETE");
    notifyClients("log:Fichier reçu (BLE). Prêt à préparer le flash.");
  }
}

void BleUpload::onDataChunk(const uint8_t* data, size_t len) {
  resetInactivityTimer();
  if (!binFile) {
    notifyClients("error:Upload non initialisé (CTRL:START_UPLOAD d'abord).");
    return;
  }
  binFile.write(data, len);
  received += len;

  if (expectedSize > 0) {
    int p = (int)((received * 100ull) / expectedSize);
    if (p != lastProgressPct) {
      notifyClients(String("log:Téléversement en cours: ") + p + "%");
      lastProgressPct = p;
    }
    if (received >= expectedSize) endUpload();
  }
}

void BleUpload::handleCtrlCommand(const std::string& s) {
  resetInactivityTimer();
  if (s.rfind("START_UPLOAD:", 0) == 0) {
    size_t total = strtoul(s.c_str() + strlen("START_UPLOAD:"), nullptr, 10);
    beginUpload(total);
    return;
  }
  if (s == "END_UPLOAD") {
    endUpload();
    return;
  }
  if (s == "CMD:PREPARE_FLASH") {
    uploader->notifyClients("log:Commande reçue (BLE). Préparation bootloader RP2040...");
    digitalWrite(BOOTLOADER_PIN, LOW);
    delay(10);
    digitalWrite(RESETRP2040_PIN, LOW);
    delay(100);
    digitalWrite(RESETRP2040_PIN, HIGH);
    delay(100);
    uploader->notifyClients("log:En attente de la réponse du RP2040...");
    startFlashProcess();
    uploader->notifyClients("EVENT:RP2040_BOOTLOADER_MODE");
    return;
  }
  if (s == "CMD:START_FLASH") {
    if (rp2040BootloaderActive) {
      digitalWrite(BOOTLOADER_PIN, HIGH);
      uploader->notifyClients("log:Démarrage du flash (BLE)...");
      startFlashProcess(SEND_INFO_COMMAND);
    } else {
      uploader->notifyClients("error:Le RP2040 n'est pas en mode bootloader.");
    }
    return;
  }
  uploader->notifyClients(String("error:Commande BLE inconnue: ") + s.c_str());
}
