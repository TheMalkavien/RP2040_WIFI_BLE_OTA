#include <LittleFS.h>
#include "esp_sleep.h"
#include "esp_ota_ops.h"
#include "config.h"
#include "rp2040_flasher.h"
#include "uploader.h"
#ifdef USE_WIFI
#include "wifi/wifi_upload.h"
#endif
#include "main.h"
#include "ble/ble_upload.h"
#include "multi_upload.h"
#if __has_include(<driver/rtc_io.h>)
#include <driver/rtc_io.h>
#define HAS_RTC_GPIO_ISOLATE 1
#endif
extern "C" {
  #include "esp_wifi.h"
  #include "esp_bt.h"
  #include "esp_pm.h"
  #include "esp_system.h"
#include <esp32/pm.h>
}
#include "ota_from_spiffs.h"

Uploader* uploader = 0;
bool rp2040BootloaderActive = false;
unsigned long lastActivityTime = 0;

void led_on() {
    #ifdef USE_RGB
        neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS,RGB_BRIGHTNESS,RGB_BRIGHTNESS);
    #else
        digitalWrite(LED_BUILTIN, LOW);
    #endif
}

void led_off() {
    #ifdef USE_RGB
        neopixelWrite(RGB_BUILTIN,0,0,0);
    #else
        digitalWrite(LED_BUILTIN, HIGH);
    #endif
}

void resetInactivityTimer() {
    noInterrupts();
    lastActivityTime = millis();
    interrupts();
    DEBUG(println("Activity detected, inactivity timer reset."));
}

void goToDeepSleep() {
    DEBUG(println("Entering deep sleep mode."));
    pinMode(WAKEUP_PIN, INPUT_PULLDOWN);
    led_off();
    delay(200);
    int level = digitalRead(BOOTLOADER_PIN);
    DEBUG(println(level ? "BOOTLOADER_PIN is HIGH, will wake up on LOW" : "BOOTLOADER_PIN is LOW, will wake up on HIGH"));

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)


    const uint64_t WAKEUP_PIN_BITMASK = 1ULL << WAKEUP_PIN;

    // Si level est HIGH (1), on se réveille sur LOW.
    // Si level est LOW (0), on se réveille sur HIGH.
    esp_deepsleep_gpio_wake_up_mode_t wakeup_mode = (level == HIGH) ? ESP_GPIO_WAKEUP_GPIO_LOW : ESP_GPIO_WAKEUP_GPIO_HIGH;

    esp_deep_sleep_enable_gpio_wakeup(WAKEUP_PIN_BITMASK, wakeup_mode);

#elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)

    esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, !level);

#else
    #error "Architecture ESP32 non supportée pour le deep sleep dans ce code."
#endif

    esp_deep_sleep_start();
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

inline void enable_pm_light_sleep() {
  esp_pm_config_esp32_t cfg{};
  cfg.max_freq_mhz = 80;
  cfg.light_sleep_enable = true;
  esp_pm_configure(&cfg);
}

void setup() {
    setCpuFrequencyMhz(80);  // 80 MHz semble être le plancher stable pour 921600
    enable_pm_light_sleep();

    SerialDBG.begin(DBG_SERIAL_BAUD);

    // Initialisation de l'UART1 pour la communication avec le RP2040 sur les broches spécifiées
    SerialRP2040.begin(RP2040_SERIAL_BAUD, SERIAL_8N1, RP2040_SERIAL_RX_PIN, RP2040_SERIAL_TX_PIN);

    delay(500); // Attendre que l'UART soit prête
    printWakeupReason();

      // Valider l'image si on est en PENDING_VERIFY
    ota_validate_running_image([](){
      // Metre ici un check simple: init FS/BLE, ping RP2040, etc.
      // Retourne true si tout va bien, sinon rollback automatique.
      DEBUG(println("OTA Reussie !"));
      return true;
    });
    // Sauf que avec le SDK Arduino, on n'a pas de mode verify, donc on peut le faire à la main, et forcer un rollback. // FIXME un jour

    printOtaInfo();
    // Configuration des broches pour le contrôle du RP2040
    pinMode(RESETRP2040_PIN, OUTPUT);
    delay(100);
    digitalWrite(RESETRP2040_PIN, HIGH);
    delay(100);
    pinMode(BOOTLOADER_PIN, OUTPUT);
    delay(100);
    digitalWrite(BOOTLOADER_PIN, HIGH);
    delay(100);

    #ifndef USE_RGB
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, LOW); // Éteindre la LED au démarrage
    #endif

    resetInactivityTimer();

    if (!LittleFS.begin()) {
        DEBUG(println("LittleFS mount failed!"));
    }

    Uploader* wifi = nullptr;
    Uploader* ble  = nullptr;

    #ifdef USE_WIFI
      wifi = new WifiUpload();
    #endif
    #ifdef USE_BLE
      ble = new BleUpload();
    #endif

    if (wifi && ble) {
      uploader = new MultiUpload(wifi, ble);
    } else if (wifi) {
      uploader = wifi;
    } else if (ble) {
      uploader = ble;
      esp_wifi_stop();
      esp_wifi_deinit();
    } else {
      while (true) { DEBUG(println("Aucun transport activé.")); delay(1000); }
    }
    uploader->Setup();


    DEBUG(println("Setup complete."));
}

void blink_led() {
    static unsigned long lastBlinkTime = 0;
    static bool ledState = false;

    unsigned long currentMillis = millis();
    if (currentMillis - lastBlinkTime >= 1000)
    {
        lastBlinkTime = currentMillis;
        ledState = !ledState;
        if (ledState) {
            led_on();
        } else {
            led_off();
        }
    }
    else if (ledState && (currentMillis - lastBlinkTime >= 100)) {
        ledState = !ledState;
        led_off();
    }
}

void loop() {
    unsigned long currentTime;
    noInterrupts();
    currentTime = millis() - lastActivityTime;
    interrupts();

    if (currentTime > INACTIVITY_TIMEOUT) {
        DEBUG(printf("Inactivity timeout reached: %lu ms. Last activity at: %lu ms\n", currentTime, lastActivityTime));
        DEBUG(println("Too much inactivity, going to deep sleep."));
        DEBUG(flush());
        goToDeepSleep();
    }
    blink_led();
    uploader->loop();
    handleFlasher(); // Appel de la machine à états dans la boucle principale
}
