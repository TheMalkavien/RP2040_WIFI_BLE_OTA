#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

// Définitions pour le bootloader RP2040
#define CMD_SYNC (('S' << 0) | ('Y' << 8) | ('N' << 16) | ('C' << 24))
#define RSP_SYNC (('P' << 0) | ('I' << 8) | ('C' << 16) | ('O' << 24))
#define CMD_WRITE (('W' << 0) | ('R' << 8) | ('I' << 16) | ('T' << 24))
#define CMD_ERASE (('E' << 0) | ('R' << 8) | ('A' << 16) | ('S' << 24))
#define CMD_CRC (('C' << 0) | ('R' << 8) | ('C' << 16) | ('C' << 24))
#define CMD_SEAL (('S' << 0) | ('E' << 8) | ('A' << 16) | ('L' << 24))
#define CMD_INFO (('I' << 0) | ('N' << 8) | ('F' << 16) | ('O' << 24))
#define CMD_GO (('G' << 0) | ('O' << 8) | ('G' << 16) | ('O' << 24))
#define RSP_OK (('O' << 0) | ('K' << 8) | ('O' << 16) | ('K' << 24))
#define RSP_ERR (('E' << 0) | ('R' << 8) | ('R' << 16) | ('!' << 24))

// Délai d'attente pour les réponses du bootloader (en ms)
#define BOOTLOADER_RESPONSE_DELAY 10

// Prototypes des fonctions
void flashFirmware(AsyncWebSocket &ws); // Gardé pour la compatibilité, mais pas utilisé directement
void flushSerial();
void sendCommandNonBlocking(const uint8_t* command, size_t len, const String& debugMessage);
uint32_t calculateCrc32(const uint8_t* data, size_t length, uint32_t crc);

// Machine à états pour le flashage non bloquant
enum FlasherState {
    IDLE,
    INIT,
    WAIT_SYNC_RESPONSE,
    SEND_INFO_COMMAND,
    WAIT_INFO_RESPONSE,
    ERASE_SECTOR,
    WAIT_ERASE_RESPONSE,
    WRITE_BLOCK,
    WAIT_WRITE_RESPONSE,
    CALCULATE_CRC, // Ajout de l'état
    SEAL_FLASH,
    WAIT_SEAL_RESPONSE,
    DONE,
    ERROR
};
void resetInactivityTimer();

// Prototypes des fonctions pour le processus de flashage
void startFlashProcess(AsyncWebSocket &ws);
void handleFlasher();
