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
    noInterrupts(); // Désactiver les interruptions
    lastActivityTime = millis();
    interrupts(); // Réactiver les interruptions
    SerialDBG.println("Activity detected, inactivity timer reset.");
}

void notifyClients(const String& message) {
    ws.textAll(message);
}

void goToDeepSleep() {
    SerialDBG.println("Entering deep sleep mode.");
    pinMode(WAKEUP_PIN, INPUT_PULLDOWN);

    delay(200);  // très important
    int level = digitalRead(BOOTLOADER_PIN);
    SerialDBG.println(level ? "BOOTLOADER_PIN is HIGH" : "BOOTLOADER_PIN is LOW");
    esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, !level);
    esp_deep_sleep_start();
}


void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    resetInactivityTimer();
    if (type == WS_EVT_CONNECT) {
        SerialDBG.printf("WebSocket client #%u connected\n", client->id());

        client->text("EVENT:MODE_UPLOADER");

    } else if (type == WS_EVT_DISCONNECT) {
        SerialDBG.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            
            if (strcmp((char*)data, "CMD:PREPARE_FLASH") == 0) {
                SerialDBG.println("PREPARE_FLASH command received. Toggling pins to enter RP2040 bootloader.");
                notifyClients("log:Commande reçue. Préparation au mode bootloader du RP2040...");
                
                // Mettre la broche BOOTLOADER_PIN à LOW pour activer le mode bootloader
                digitalWrite(BOOTLOADER_PIN, LOW);
                delay(10);
                
                // Activer le RESET du RP2040
                digitalWrite(RESETRP2040_PIN, LOW);
                delay(100);
                digitalWrite(RESETRP2040_PIN, HIGH);
                delay(100);
                
                // Relâcher la broche BOOTLOADER_PIN
                //digitalWrite(BOOTLOADER_PIN, HIGH);

                rp2040BootloaderActive = true;
                notifyClients("EVENT:RP2040_BOOTLOADER_MODE");
            }

            if (strcmp((char*)data, "CMD:START_FLASH") == 0) {
                 if (rp2040BootloaderActive) {
                    // Relâcher la broche BOOTLOADER_PIN
                    digitalWrite(BOOTLOADER_PIN, HIGH);
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
    static File binFile;
    if (!index) {
        notifyClients("log:Début du téléversement...");
        binFile = LittleFS.open("/firmware.bin", "w");
        if (!binFile) {
            notifyClients("error:Impossible d'ouvrir le fichier sur l'ESP32.");
            return;
        }
    }
    if (len) {
        binFile.write(data, len);
    }
    if (final) {
        binFile.close();
        notifyClients("EVENT:UPLOAD_COMPLETE");
        notifyClients("log:Fichier reçu. Prêt à préparer le flash.");
    }

}

void printWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  Serial.print("Wakeup reason: ");
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("EXT0 (RTC GPIO)");
      break;

    case ESP_SLEEP_WAKEUP_EXT1: {
      Serial.println("EXT1 (multiple GPIOs)");
      uint64_t wakeup_gpio_mask = esp_sleep_get_ext1_wakeup_status();
      Serial.print("  Wakeup GPIO mask: 0x");
      Serial.println((uint32_t)(wakeup_gpio_mask), HEX);

      // (optionnel) afficher quels GPIOs sont en cause
      for (int i = 0; i < 64; ++i) {
        if (wakeup_gpio_mask & (1ULL << i)) {
          Serial.printf("  GPIO %d triggered wake-up\n", i);
        }
      }
      break;
    }

    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Timer");
      break;

    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Touchpad");
      break;

    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("ULP");
      break;

    case ESP_SLEEP_WAKEUP_GPIO:
      Serial.println("GPIO (legacy mode)");
      break;

    case ESP_SLEEP_WAKEUP_UART:
      Serial.println("UART");
      break;

    case ESP_SLEEP_WAKEUP_ALL:
      Serial.println("All (should not happen)");
      break;

    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      Serial.println("Undefined");
      break;
  }
}

void setup() {
    // Initialisation de l'UART pour le débogage
    SerialDBG.begin(DBG_SERIAL_BAUD);
    
    // Initialisation de l'UART1 pour la communication avec le RP2040 sur les broches spécifiées
    Serial1.begin(RP2040_SERIAL_BAUD, SERIAL_8N1, RP2040_SERIAL_RX_PIN, RP2040_SERIAL_TX_PIN);
    //pinMode(RP2040_SERIAL_RX_PIN, INPUT);
    //pinMode(RP2040_SERIAL_TX_PIN, INPUT);
    delay(100); // Attendre que l'UART soit prête
    printWakeupReason();

    // Configuration des broches pour le contrôle du RP2040
    pinMode(BOOTLOADER_PIN, OUTPUT);
    digitalWrite(BOOTLOADER_PIN, HIGH);
    pinMode(RESETRP2040_PIN, OUTPUT);
    digitalWrite(RESETRP2040_PIN, HIGH);
    pinMode(BOOTLOADER_PIN, OUTPUT);
    digitalWrite(BOOTLOADER_PIN, HIGH);

    #ifdef LED_BUILTIN

    #else
        pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, LOW); // Éteindre la LED au démarrage
    #endif

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

void blink_led() {
    static unsigned long lastBlinkTime = 0;
    static bool ledState = false;

    unsigned long currentMillis = millis();
    if (currentMillis - lastBlinkTime >= 1000) { // 1 seconde
        lastBlinkTime = currentMillis;
        ledState = !ledState; // Inverser l'état de la LED
        #ifdef LED_BUILTIN
            if (ledState)
            {
                neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS,RGB_BRIGHTNESS,RGB_BRIGHTNESS);
                lastBlinkTime -= 900; // Ajuster le temps pour éviter le clignotement trop lent
            }
            else
                neopixelWrite(RGB_BUILTIN,0,0,0);
        #else
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        #endif
    }
}

void loop() {
    unsigned long currentTime;
    noInterrupts(); // Désactiver les interruptions
    currentTime = millis() - lastActivityTime;
    interrupts(); // Réactiver les interruptions

    if (currentTime > INACTIVITY_TIMEOUT) {
        SerialDBG.printf("Inactivity timeout reached: %lu ms. Last activity at: %lu ms\n", currentTime, lastActivityTime);
        SerialDBG.println("Too much inactivity, going to deep sleep.");
        SerialDBG.flush();
        goToDeepSleep();
    }

    blink_led();

    ws.cleanupClients();
    handleFlasher(); // Appel de la machine à états dans la boucle principale
}
