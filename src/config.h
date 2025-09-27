#pragma once

#define SSID "ESP32-Uploader"
#define PASSWORD "12345678"

#define DBG_SERIAL_BAUD 115200


#define SerialDBG Serial
#define SerialRP2040 Serial1
#define DEBUG(x) SerialDBG.x


#define WAKEUP_PIN GPIO_NUM_3 // same as BOOTLOADER PIN 
#define RESETRP2040_PIN GPIO_NUM_2
#define BOOTLOADER_PIN GPIO_NUM_3 // to GPIO22 of the RP2040
#define INACTIVITY_TIMEOUT (5 * 60 * 1000)

#ifndef RP2040_SERIAL_TX_PIN
#define RP2040_SERIAL_TX_PIN TX // to the GPIO9 of the RP2040
#endif
#ifndef RP2040_SERIAL_RX_PIN
#define RP2040_SERIAL_RX_PIN RX // to the GPIO8 of the RP2040
#endif

#define RP2040_SERIAL_BAUD 921600

#ifdef RGB_BUILTIN
#undef RGB_BUILTIN
#undef RGB_BRIGHTNESS
#define RGB_BRIGHTNESS 10 // Change white brightness (max 255)
#define RGB_BUILTIN 21
#endif
