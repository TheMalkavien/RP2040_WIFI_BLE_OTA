#include "wifi_upload.h"
#include "config.h"
#include "main.h"
#include "rp2040_flasher.h"


WifiUpload::WifiUpload() {
    server = new AsyncWebServer(80);
    ws = new AsyncWebSocket("/ws");
}   

WifiUpload::~WifiUpload() {
    delete server;
    delete ws;
}   

void WifiUpload::notifyClients(const String& message) {
    ws->textAll(message);
}

void WifiUpload::Setup() {
    WiFi.softAP(MY_SSID, MY_PASSWORD);
    DEBUG(print("AP IP address: "));
    DEBUG(println(WiFi.softAPIP()));

    ws->onEvent(onWsEvent);
    server->addHandler(ws);


    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    server->on("/", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200);
    }, handleUpload);

    server->begin();
}

int onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    resetInactivityTimer();
    if (type == WS_EVT_CONNECT) {
        DEBUG(printf("WebSocket client #%u connected\n", client->id()));

        client->text("EVENT:MODE_UPLOADER");

    } else if (type == WS_EVT_DISCONNECT) {
        DEBUG(printf("WebSocket client #%u disconnected\n", client->id()));
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            // Reboot RP2040: simple pulse sur la broche reset du RP2040
            if (strcmp((char*)data, "CMD:REBOOT_RP2040") == 0) {
                uploader->notifyClients("log:Reboot RP2040...");
                digitalWrite(RESETRP2040_PIN, LOW);
                delay(100);
                digitalWrite(RESETRP2040_PIN, HIGH);
                delay(100);
                uploader->notifyClients("success:RP2040 redémarré.");
                return 0;
            }

            // Reboot ESP32: redémarre l'hôte
            if (strcmp((char*)data, "CMD:REBOOT_ESP32") == 0) {
                uploader->notifyClients("log:Reboot ESP32...");
                delay(50);
                ESP.restart();
                return 0; // ne sera probablement jamais atteint
            }

            if (strcmp((char*)data, "CMD:PREPARE_FLASH") == 0) {
                DEBUG(println("PREPARE_FLASH command received. Toggling pins to enter RP2040 bootloader."));
                uploader->notifyClients("log:Commande reçue. Préparation au mode bootloader du RP2040...");
                
                // Mettre la broche BOOTLOADER_PIN à LOW pour activer le mode bootloader
                digitalWrite(BOOTLOADER_PIN, LOW);
                delay(10);
                
                // Activer le RESET du RP2040
                digitalWrite(RESETRP2040_PIN, LOW);
                delay(100);
                digitalWrite(RESETRP2040_PIN, HIGH);
                delay(100);
                uploader->notifyClients("log:En attente de la réponse du RP2040...");
                startFlashProcess(); // Appel de la nouvelle fonction pour démarrer la machine à états

                uploader->notifyClients("EVENT:RP2040_BOOTLOADER_MODE");
            }

            if (strcmp((char*)data, "CMD:START_FLASH") == 0) {
                 if (rp2040BootloaderActive) {
                    // Relâcher la broche BOOTLOADER_PIN
                    digitalWrite(BOOTLOADER_PIN, HIGH);
                    uploader->notifyClients("log:Démarrage du processus de flashage...");
                    startFlashProcess(SEND_INFO_COMMAND); // Appel de la nouvelle fonction pour démarrer la machine à états
                 } else {
                    uploader->notifyClients("error:Le RP2040 n'est pas en mode bootloader.");
                 }
            }
        }
    }
    return 0;
}

void WifiUpload::handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    resetInactivityTimer();
    static File binFile;
    if (!index) {
        uploader->notifyClients("log:Début du téléversement...");
        binFile = LittleFS.open("/firmware.bin", "w");
        if (!binFile) {
            uploader->notifyClients("error:Impossible d'ouvrir le fichier sur l'ESP32.");
            return;
        }
    }
    if (len) {
        binFile.write(data, len);
    }
    if (final) {
        binFile.close();
        uploader->notifyClients("EVENT:UPLOAD_COMPLETE");
        uploader->notifyClients("log:Fichier reçu. Prêt à préparer le flash.");
    }

}
void WifiUpload::loop() {
    ws->cleanupClients();
}