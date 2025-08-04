#include "rp2040_flasher.h"
#include "config.h"

// Définition de la fonction de notification pour le module
extern void notifyClients(const String& message);

#define VTOR 0x10004000
#define ALIGN_UP(val, align) (((val) + ((align) - 1)) & ~((align) - 1))
// Variables globales pour le processus de flashage
FlasherState flasherState = IDLE;
AsyncWebSocket* webSocketPtr;
File uf2File;
uint32_t fileSize = 0;
uint8_t filebuffer[4096];
uint32_t currentFilePosition = 0;
uint32_t currentEraseAddress = 0;
uint32_t flashStart = 0;
unsigned long stateStartTime = 0;
uint32_t eraseSize = 0;
uint32_t writeSize = 0;
uint32_t currentWriteOffset = 0;
unsigned long commandSentTime = 0;
int lastProgress = 0;

// Variables pour le calcul du CRC
uint32_t calculatedCrc = 0;

// Fonction pour vider le buffer série d'entrée
void flushSerial() {
    while (Serial1.available()) {
        Serial1.read();
    }
}

// Nouvelle fonction non bloquante pour envoyer une commande
void sendCommandNonBlocking(const uint8_t* command, size_t len, const String& debugMessage = "") {
    flushSerial();
    if (command && len > 0) {
        SerialDBG.printf("Sending command: 0x%08X", *(uint32_t*)command);
        if (len > 4) {
            SerialDBG.printf(" with args:");
            for (size_t i = 4; i < len; i += 4) {
                SerialDBG.printf(" 0x%08X", *(uint32_t*)(command + i));
            }
        }
        SerialDBG.println();
        Serial1.write(command, len);
        Serial1.flush();
    }
    commandSentTime = millis();
}

// Fonction utilitaire de calcul du CRC32 (implémentation simple)
uint32_t calculateCrc32(const uint8_t* data, size_t length, uint32_t crc = 0xFFFFFFFF) {
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Fonction qui calcule le CRC32 d'un fichier entier
uint32_t calculateCrc32FromFile(File& file) {
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buffer[512]; // Buffer de lecture
    uint32_t bytesRead = 0;
    
    file.seek(0);

    while(file.position() < file.size()) {
        bytesRead = file.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            crc = calculateCrc32(buffer, bytesRead, crc);
        } else {
            // Fin du fichier ou erreur de lecture
            break;
        }
    }
    
    return ~crc;
}

// Fonction pour initialiser le processus de flashage
void startFlashProcess(AsyncWebSocket &ws) {
    webSocketPtr = &ws;
    flasherState = INIT;
    notifyClients("log:Démarrage du processus de flashage...");
    stateStartTime = millis();
    lastProgress = 0;
}

// Machine à états pour le flashage non bloquant
void handleFlasher() {
    switch (flasherState) {
        case IDLE:
            break;

        case INIT: {
            if (millis() - stateStartTime < 1000) {
                return;
            }
            uf2File = LittleFS.open("/firmware.bin", "r");
            if (!uf2File) {
                notifyClients("error:Fichier firmware.bin introuvable.");
                flasherState = ERROR;
                return;
            }
            fileSize = uf2File.size();
            currentFilePosition = 0;
            notifyClients("log:Synchronisation avec le bootloader du RP2040...");
            uint32_t syncCmd = CMD_SYNC;
            sendCommandNonBlocking((uint8_t*)&syncCmd, sizeof(syncCmd));
            flasherState = WAIT_SYNC_RESPONSE;
            break;
        }
        case WAIT_SYNC_RESPONSE: {
            if (millis() - commandSentTime < BOOTLOADER_RESPONSE_DELAY) {
                return;
            }
            if (Serial1.available() >= 4) {
                uint32_t response;
                Serial1.readBytes((uint8_t*)&response, 4);
                if (response != RSP_SYNC) {
                    notifyClients("error:Réponse de synchronisation inattendue.");
                    SerialDBG.printf("Error: Unexpected SYNC response. Expected: 0x%08X, Received: 0x%08X\n", RSP_SYNC, response);
                    flasherState = INIT;
                } else {
                    notifyClients("log:Synchronisation réussie.");
                    SerialDBG.printf("Response OK: 0x%08X\n", response);
                    flasherState = SEND_INFO_COMMAND;
                }
            } else if (millis() - stateStartTime > 5000) {
                 notifyClients("error:Timeout lors de l'attente de la réponse de synchronisation.");
                 SerialDBG.println("Error: Timeout waiting for SYNC response.");
                 flasherState = ERROR;
            }
            break;
        }

        case SEND_INFO_COMMAND: {
            notifyClients("log:Récupération des informations sur la flash...");
            uint32_t infoCmd = CMD_INFO;
            sendCommandNonBlocking((uint8_t*)&infoCmd, sizeof(infoCmd));
            flasherState = WAIT_INFO_RESPONSE;
            break;
        }

        case WAIT_INFO_RESPONSE: {
            if (millis() - commandSentTime < BOOTLOADER_RESPONSE_DELAY) {
                return;
            }
            if (Serial1.available() >= (4 + 5 * sizeof(uint32_t))) {
                uint32_t response;
                uint32_t infoData[5];
                Serial1.readBytes((uint8_t*)&response, 4);
                Serial1.readBytes((uint8_t*)&infoData, 5 * sizeof(uint32_t));
                if (response != RSP_OK) {
                    notifyClients("error:Erreur lors de la récupération des informations sur la flash.");
                    SerialDBG.printf("Error: Unexpected INFO response. Expected: 0x%08X, Received: 0x%08X\n", RSP_OK, response);
                    flasherState = ERROR;
                } else {
                    eraseSize = infoData[2];
                    writeSize = infoData[4];
                    
                    notifyClients(String("log:Flash info: Flash Start: 0x") + String(infoData[0], HEX) + 
                                  ", Flash Size: " + String(infoData[1], HEX) + 
                                  ", Erase Size: " + String(eraseSize, HEX) + 
                                  ", Write Size: " + String(writeSize, HEX) + 
                                  ", Max Data Len: " + String(infoData[4], HEX));
                    
                    SerialDBG.printf("Flash info: Flash Start: 0x%08X, Flash Size: 0x%08X, Erase Size: 0x%08X, Write Size: 0x%08X, Max Data Len: 0x%08X\n",
                                    infoData[0], infoData[1], eraseSize, writeSize, infoData[4]);
                    flashStart = infoData[0];
                    currentEraseAddress = flashStart;
                    flasherState = ERASE_SECTOR;
                }
            } else if (millis() - stateStartTime > 5000) {
                 notifyClients("error:Timeout lors de l'attente des informations sur la flash.");
                 SerialDBG.println("Error: Timeout waiting for INFO response.");
                 flasherState = ERROR;
            }
            break;
        }

        case ERASE_SECTOR: {
            if (currentEraseAddress >= (flashStart + fileSize)) {
                notifyClients("log:Effacement terminé.");
                SerialDBG.println("Flash erase complete.");
                currentFilePosition = 0;
                lastProgress = 0;
                flasherState = WRITE_BLOCK;
                return;
            }
            
            uint32_t eraseCmd[3];
            eraseCmd[0] = CMD_ERASE;
            eraseCmd[1] = currentEraseAddress;
            eraseCmd[2] = eraseSize;
            sendCommandNonBlocking((uint8_t*)&eraseCmd, sizeof(eraseCmd));
            flasherState = WAIT_ERASE_RESPONSE;
            break;
        }

        case WAIT_ERASE_RESPONSE: {
            if (millis() - commandSentTime < BOOTLOADER_RESPONSE_DELAY) {
                return;
            }
            if (Serial1.available() >= 4) {
                uint32_t response;
                Serial1.readBytes((uint8_t*)&response, 4);
                if (response != RSP_OK) {
                    notifyClients(String("error:Erreur lors de l'effacement à l'adresse 0x") + String(currentEraseAddress, HEX));
                    SerialDBG.printf("Error: Unexpected ERASE response. Expected: 0x%08X, Received: 0x%08X\n", RSP_OK, response);
                    flasherState = ERROR;
                } else {
                    currentEraseAddress += eraseSize;
                    int progress = ((currentEraseAddress - flashStart) * 100) / fileSize;
                    if (progress > lastProgress) {
                        lastProgress = progress;
                        notifyClients(String("log:Effacement en cours: ") + progress + "%");
                    }
                    SerialDBG.printf("Erase block OK. Progress: %d%%\n", progress);
                    flasherState = ERASE_SECTOR;
                }
            } else if (millis() - commandSentTime > 5000) {
                 notifyClients("error:Timeout lors de l'attente de la réponse de l'effacement.");
                 SerialDBG.println("Error: Timeout waiting for ERASE response.");
                 flasherState = ERROR;
            }
            break;
        }
        
        case WRITE_BLOCK: {
            if (currentFilePosition >= fileSize) {
                flasherState = CALCULATE_CRC;
                return;
            }
            uf2File.seek(currentFilePosition); 
            uint32_t r = uf2File.read(filebuffer, writeSize); 
            uint32_t towrite = r;
            if (r <= 0) 
            {
                notifyClients("error:Erreur de lecture du fichier UF2.");
                flasherState = ERROR;
                return;
            }
            towrite = ALIGN_UP(towrite, 256);
            uint32_t writeCmd[3];
            writeCmd[0] = CMD_WRITE;
            writeCmd[1] = flashStart + currentFilePosition;
            writeCmd[2] = towrite;
            
            SerialDBG.printf("Sending WRITE command. Address: 0x%08X, Size: 0x%08X\n", writeCmd[1], writeCmd[2]);
            Serial1.write((uint8_t*)&writeCmd, sizeof(writeCmd));
            Serial1.write(filebuffer, towrite);
            Serial1.flush();
            
            currentFilePosition += towrite;
            commandSentTime = millis();
            flasherState = WAIT_WRITE_RESPONSE;
            break;
        }

        case WAIT_WRITE_RESPONSE: {
            if (millis() - commandSentTime < BOOTLOADER_RESPONSE_DELAY) {
                return;
            }
            if (Serial1.available() >= 8) {
                uint32_t response;
                uint32_t crc;
                
                Serial1.readBytes((uint8_t*)&response, 4);
                Serial1.readBytes((uint8_t*)&crc, 4);
                if (response != RSP_OK) {
                    notifyClients("error:Erreur lors de l'écriture du bloc.");
                    SerialDBG.printf("Error: Unexpected WRITE response. Expected: 0x%08X, Received: 0x%08X with crc : 0x%08X\n", RSP_OK, response, crc);
                    flasherState = ERROR;
                } else {
                    int progress = (currentFilePosition * 100) / fileSize;
                    if (progress > lastProgress) {
                        lastProgress = progress;
                        notifyClients(String("log:Flashage en cours: ") + progress + "%");
                    }
                    SerialDBG.printf("Write block OK. Progress: %d%%\n", progress);
                    flasherState = WRITE_BLOCK;
                }
            } else if (millis() - commandSentTime > 5000) {
                 notifyClients("error:Timeout lors de l'attente de la réponse de l'écriture.");
                 SerialDBG.println("Error: Timeout waiting for WRITE response.");
                 flasherState = ERROR;
            }
            break;
        }

        case CALCULATE_CRC: {
            notifyClients("log:Calcul du CRC du firmware...");
            uf2File.seek(0);
            calculatedCrc = calculateCrc32FromFile(uf2File);
            notifyClients(String("log:CRC calculé : 0x") + String(calculatedCrc, HEX));
            flasherState = SEAL_FLASH;
            break;
        }


        case SEAL_FLASH: {
            notifyClients("log:Scellement du firmware...");
            uint32_t sealCmd[4];
            sealCmd[0] = CMD_SEAL;
            sealCmd[1] = flashStart;
            sealCmd[2] = fileSize;
            sealCmd[3] = calculatedCrc;
            sendCommandNonBlocking((uint8_t*)&sealCmd, sizeof(sealCmd));
            flasherState = WAIT_SEAL_RESPONSE;
            break;
        }

        case WAIT_SEAL_RESPONSE: {
            if (millis() - commandSentTime < BOOTLOADER_RESPONSE_DELAY) {
                return;
            }
            if (Serial1.available() >= 4) {
                uint32_t response;
                Serial1.readBytes((uint8_t*)&response, 4);
                if (response != RSP_OK) {
                    notifyClients("error:Erreur lors du scellement.");
                    SerialDBG.printf("Error: Unexpected SEAL response. Expected: 0x%08X, Received: 0x%08X\n", RSP_OK, response);
                    flasherState = ERROR;
                } else {
                    notifyClients("log:Scellement réussi.");
                    SerialDBG.printf("Response OK: 0x%08X\n", response);
                    flasherState = DONE;
                }
            } else if (millis() - commandSentTime > 5000) {
                 notifyClients("error:Timeout lors de l'attente de la réponse du scellement.");
                 SerialDBG.println("Error: Timeout waiting for SEAL response.");
                 flasherState = ERROR;
            }
            break;
        }

        case DONE:
            notifyClients("log:Flashage terminé ! L'appareil va redémarrer.");
            notifyClients("EVENT:FLASH_COMPLETE");
            uf2File.close();
            uint32_t goCmd[2];
            goCmd[0] = CMD_GO;
            goCmd[1] = flashStart;
            sendCommandNonBlocking((uint8_t*)&goCmd, sizeof(goCmd));
            flasherState = IDLE;
            break;

        case ERROR:
            uf2File.close();
            flasherState = IDLE;
            break;
    }
}
