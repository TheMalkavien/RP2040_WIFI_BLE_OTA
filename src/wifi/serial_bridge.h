#pragma once
#include <Arduino.h>
#include <WiFi.h>
#ifdef USE_WIFI
#include <ESPAsyncWebServer.h>
#endif

// Appeler une fois au setup après le Wi-Fi déjà connecté
void serialBridgeBegin();

// Appeler dans loop() pour maintenir le pont actif
void serialBridgeLoop();

#ifdef USE_WIFI
// WebSocket de la console série (page serial.html), à enregistrer sur le
// serveur web avec server->addHandler(serialConsoleWs()).
// Protocole: trames binaires = octets UART bruts (dans les deux sens),
//            trames texte    = commandes "CMD:..." / notifications "INFO:...".
AsyncWebSocket* serialConsoleWs();
#endif

// Baudrate courant de l'UART RP2040 (modifiable depuis la console série).
// La demande est appliquée dans serialBridgeLoop(), jamais depuis la tâche
// async du serveur web.
uint32_t serialConsoleBaud();
bool serialConsoleRequestBaud(uint32_t baud);
