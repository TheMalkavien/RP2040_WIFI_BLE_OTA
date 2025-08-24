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

void led_on() {
    #ifdef RGB_BUILTIN
        neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS,RGB_BRIGHTNESS,RGB_BRIGHTNESS);
    #else
        digitalWrite(LED_BUILTIN, HIGH); // Allumer la LED
    #endif
}

void led_off() {
    #ifdef RGB_BUILTIN
        neopixelWrite(RGB_BUILTIN,0,0,0);
    #else
        digitalWrite(LED_BUILTIN, LOW); // Éteindre la LED
    #endif
}

void resetInactivityTimer() {
    noInterrupts(); // Désactiver les interruptions
    lastActivityTime = millis();
    interrupts(); // Réactiver les interruptions
    DEBUG(println("Activity detected, inactivity timer reset."));
}

void notifyClients(const String& message) {
    ws.textAll(message);
}

void goToDeepSleep() {
    DEBUG(println("Entering deep sleep mode."));
    pinMode(WAKEUP_PIN, INPUT_PULLDOWN);
    led_off();
    delay(200);
    int level = digitalRead(BOOTLOADER_PIN);
    DEBUG(println(level ? "BOOTLOADER_PIN is HIGH, will wake up on LOW" : "BOOTLOADER_PIN is LOW, will wake up on HIGH"));

    // --- Début du code spécifique à l'architecture ---

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    // Ce bloc est pour les ESP32-C3, C6, H2 etc. (qui n'ont pas ext0)

    const uint64_t WAKEUP_PIN_BITMASK = 1ULL << WAKEUP_PIN;

    // On recrée la logique '!level' :
    // Si level est HIGH (1), on se réveille sur LOW.
    // Si level est LOW (0), on se réveille sur HIGH.
    esp_deepsleep_gpio_wake_up_mode_t wakeup_mode = (level == HIGH) ? ESP_GPIO_WAKEUP_GPIO_LOW : ESP_GPIO_WAKEUP_GPIO_HIGH;

    esp_deep_sleep_enable_gpio_wakeup(WAKEUP_PIN_BITMASK, wakeup_mode);

#elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    // Ce bloc est pour les ESP32 classiques, S2 et S3 (qui ont tous ext0)

    esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, !level);

#else
    #error "Architecture ESP32 non supportée pour le deep sleep dans ce code."
#endif

    // --- Fin du code spécifique à l'architecture ---

    esp_deep_sleep_start();
}


void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
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
            
            if (strcmp((char*)data, "CMD:PREPARE_FLASH") == 0) {
                DEBUG(println("PREPARE_FLASH command received. Toggling pins to enter RP2040 bootloader."));
                notifyClients("log:Commande reçue. Préparation au mode bootloader du RP2040...");
                
                // Mettre la broche BOOTLOADER_PIN à LOW pour activer le mode bootloader
                digitalWrite(BOOTLOADER_PIN, LOW);
                delay(10);
                
                // Activer le RESET du RP2040
                digitalWrite(RESETRP2040_PIN, LOW);
                delay(100);
                digitalWrite(RESETRP2040_PIN, HIGH);
                delay(100);
                notifyClients("log:En attente de la réponse du RP2040...");
                startFlashProcess(ws); // Appel de la nouvelle fonction pour démarrer la machine à états

                notifyClients("EVENT:RP2040_BOOTLOADER_MODE");
            }

            if (strcmp((char*)data, "CMD:START_FLASH") == 0) {
                 if (rp2040BootloaderActive) {
                    // Relâcher la broche BOOTLOADER_PIN
                    digitalWrite(BOOTLOADER_PIN, HIGH);
                    notifyClients("log:Démarrage du processus de flashage...");
                    startFlashProcess(ws, SEND_INFO_COMMAND); // Appel de la nouvelle fonction pour démarrer la machine à états
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
      uint64_t wakeup_gpio_mask;

      #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
      // Pour C3, C6, etc., on utilise la fonction GPIO unifiée
      Serial.println("Wakeup caused by GPIO (unified)");
      wakeup_gpio_mask = esp_sleep_get_gpio_wakeup_status();

#elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
      // Pour ESP32 classique, S2, S3, on utilise la fonction EXT1
      Serial.println("Wakeup caused by external signal using RTC_CNTL (EXT1)");
      wakeup_gpio_mask = esp_sleep_get_ext1_wakeup_status();
#endif
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
    setCpuFrequencyMhz(80);  // 80 MHz semble être le plancher stable pour 921600
    // Initialisation de l'UART pour le débogage

    SerialDBG.begin(DBG_SERIAL_BAUD);

    // Initialisation de l'UART1 pour la communication avec le RP2040 sur les broches spécifiées
    SerialRP2040.begin(RP2040_SERIAL_BAUD, SERIAL_8N1, RP2040_SERIAL_RX_PIN, RP2040_SERIAL_TX_PIN);

    //pinMode(RP2040_SERIAL_RX_PIN, INPUT);
    //pinMode(RP2040_SERIAL_TX_PIN, INPUT);
    delay(500); // Attendre que l'UART soit prête
    printWakeupReason();

    // Configuration des broches pour le contrôle du RP2040
    pinMode(BOOTLOADER_PIN, OUTPUT);
    digitalWrite(BOOTLOADER_PIN, HIGH);
    pinMode(RESETRP2040_PIN, OUTPUT);
    digitalWrite(RESETRP2040_PIN, HIGH);
    pinMode(BOOTLOADER_PIN, OUTPUT);
    digitalWrite(BOOTLOADER_PIN, HIGH);

    #ifndef RGB_BUILTIN
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, LOW); // Éteindre la LED au démarrage
    #endif

    resetInactivityTimer();

    if (!LittleFS.begin()) {
        DEBUG(println("LittleFS mount failed!"));
    }

    WiFi.softAP(SSID, PASSWORD);
    DEBUG(print("AP IP address: "));
    DEBUG(println(WiFi.softAPIP()));

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);


    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200);
    }, handleUpload);

    server.begin();
    DEBUG(println("Setup complete."));
}

void blink_led() {
    static unsigned long lastBlinkTime = 0;
    static bool ledState = false;

    unsigned long currentMillis = millis();
    if (currentMillis - lastBlinkTime >= 1000)
    {
        lastBlinkTime = currentMillis;
        ledState = !ledState; // Inverser l'état de la LED
        if (ledState) {
            led_on();
        } else {
            led_off();
        }
    }
}

void loop() {
    unsigned long currentTime;
    noInterrupts(); // Désactiver les interruptions
    currentTime = millis() - lastActivityTime;
    interrupts(); // Réactiver les interruptions

    if (currentTime > INACTIVITY_TIMEOUT) {
        DEBUG(printf("Inactivity timeout reached: %lu ms. Last activity at: %lu ms\n", currentTime, lastActivityTime));
        DEBUG(println("Too much inactivity, going to deep sleep."));
        DEBUG(flush());
        goToDeepSleep();
    }

    blink_led();

    ws.cleanupClients();
    handleFlasher(); // Appel de la machine à états dans la boucle principale
}
