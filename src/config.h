#pragma once

#define SSID "ESP32-Uploader"
#define PASSWORD "12345678"

#define DBG_SERIAL_TX 43
#define DBG_SERIAL_RX 44
#define DBG_SERIAL_BAUD 115200
#define SerialDBG Serial
#define WAKEUP_PIN GPIO_NUM_1
#define RESETRP2040_PIN GPIO_NUM_2
#define BOOTLOADER_PIN GPIO_NUM_3 // Nouvelle broche pour le mode bootloader
#define INACTIVITY_TIMEOUT (10 * 60 * 1000)

// Nouvelles broches pour la communication avec le RP2040
#define RP2040_SERIAL_TX_PIN 7
#define RP2040_SERIAL_RX_PIN 8
#define RP2040_SERIAL_BAUD 250000
