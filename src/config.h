#pragma once

#define SSID "ESP32-Uploader"
#define PASSWORD "12345678"

#define DBG_SERIAL_TX 21 // 43 for USB
#define DBG_SERIAL_RX 20 // 44 for USB
#define DBG_SERIAL_BAUD 115200
#define SerialDBG Serial
#define WAKEUP_PIN GPIO_NUM_3 // same as BOOTLOADER PIN 
#define RESETRP2040_PIN GPIO_NUM_2
#define BOOTLOADER_PIN GPIO_NUM_3 // Nouvelle broche pour le mode bootloader
#define INACTIVITY_TIMEOUT (1 * 60 * 1000)

#define LED_PIN GPIO_NUM_21

// Nouvelles broches pour la communication avec le RP2040
#define RP2040_SERIAL_TX_PIN 7
#define RP2040_SERIAL_RX_PIN 8
#define RP2040_SERIAL_BAUD 921600




#ifdef RGB_BUILTIN
#undef RGB_BUILTIN
#undef RGB_BRIGHTNESS
#define RGB_BRIGHTNESS 16 // Change white brightness (max 255)
#define RGB_BUILTIN 21
#endif
