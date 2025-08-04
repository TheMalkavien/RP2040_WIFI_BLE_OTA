#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include "esp_sleep.h"
#include "esp_ota_ops.h"
#include "config.h"
#include "rp2040_flasher.h" // Inclusion du nouveau module

unsigned long lastActivityTime = 0;
bool rp2040BootloaderActive = false;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void resetInactivityTimer() {
    lastActivityTime = millis();
    SerialDBG.println("Activity detected, inactivity timer reset.");
}

void notifyClients(const String& message) {
    ws.textAll(message);
}

void goToDeepSleep() {
    SerialDBG.println("Entering deep sleep mode.");
    delay(100);
    esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 1);
    esp_deep_sleep_start();
}


void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    resetInactivityTimer();
    if (type == WS_EVT_CONNECT) {
        SerialDBG.printf("WebSocket client #%u connected\n", client->id());
        #ifdef APP_UPLOADER
            client->text("EVENT:MODE_UPLOADER");
        #elif APP_FLASHER
            client->text("EVENT:MODE_FLASHER");
        #endif
    } else if (type == WS_EVT_DISCONNECT) {
        SerialDBG.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            
            #ifdef APP_UPLOADER
            if (strcmp((char*)data, "CMD:PREPARE_FLASH") == 0) {
                SerialDBG.println("PREPARE_FLASH command received. Toggling pins to enter RP2040 bootloader.");
                notifyClients("log:Commande reçue. Préparation au mode bootloader du RP2040...");
                
                // Mettre la broche BOOTLOADER_PIN à HIGH
                digitalWrite(BOOTLOADER_PIN, HIGH);
                delay(10);
                
                // Activer le RESET du RP2040
                digitalWrite(RESETRP2040_PIN, LOW);
                delay(100);
                digitalWrite(RESETRP2040_PIN, HIGH);
                delay(100);
                
                // Relâcher la broche BOOTLOADER_PIN
                digitalWrite(BOOTLOADER_PIN, LOW);
                
                rp2040BootloaderActive = true;
                notifyClients("EVENT:RP2040_BOOTLOADER_MODE");
            }
            #endif

            if (strcmp((char*)data, "CMD:START_FLASH") == 0) {
                 if (rp2040BootloaderActive) {
                    startFlashProcess(ws); // Appel de la nouvelle fonction pour démarrer la machine à états
                 } else {
                    notifyClients("error:Le RP2040 n'est pas en mode bootloader.");
                 }
            }
        }
    }
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    resetInactivityTimer();
    static File uf2File;
    if (!index) {
        notifyClients("log:Début du téléversement...");
        uf2File = LittleFS.open("/firmware.bin", "w");
        if (!uf2File) {
            notifyClients("error:Impossible d'ouvrir le fichier sur l'ESP32.");
            return;
        }
    }
    if (len) {
        uf2File.write(data, len);
    }
    if (final) {
        uf2File.close();
        notifyClients("EVENT:UPLOAD_COMPLETE");
        notifyClients("log:Fichier reçu. Prêt à préparer le flash.");
    }

}

void setup() {
    // Initialisation de l'UART pour le débogage
    SerialDBG.begin(DBG_SERIAL_BAUD);
    
    // Initialisation de l'UART1 pour la communication avec le RP2040 sur les broches spécifiées
    Serial1.begin(RP2040_SERIAL_BAUD, SERIAL_8N1, RP2040_SERIAL_RX_PIN, RP2040_SERIAL_TX_PIN);
    
    delay(500);
    
    // Configuration des broches pour le contrôle du RP2040
    pinMode(BOOTLOADER_PIN, OUTPUT);
    digitalWrite(BOOTLOADER_PIN, LOW);
    pinMode(RESETRP2040_PIN, OUTPUT);
    digitalWrite(RESETRP2040_PIN, HIGH);

    pinMode(WAKEUP_PIN, INPUT_PULLDOWN);
    if (digitalRead(WAKEUP_PIN) == LOW) {
        goToDeepSleep();
    }
    resetInactivityTimer();

    if (!LittleFS.begin()) {
        SerialDBG.println("LittleFS mount failed!");
    }

    WiFi.softAP(SSID, PASSWORD);
    SerialDBG.print("AP IP address: ");
    SerialDBG.println(WiFi.softAPIP());

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);


    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200);
    }, handleUpload);

    server.begin();
    SerialDBG.println("Setup complete.");
}

void loop() {
    if (digitalRead(WAKEUP_PIN) == LOW && (millis() - lastActivityTime > INACTIVITY_TIMEOUT)) {
        goToDeepSleep();
    }

    ws.cleanupClients();
    handleFlasher(); // Appel de la machine à états dans la boucle principale
}
