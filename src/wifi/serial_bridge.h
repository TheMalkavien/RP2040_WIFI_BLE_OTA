#pragma once
#include <Arduino.h>
#include <WiFi.h>

// Appeler une fois au setup après le Wi-Fi déjà connecté
void serialBridgeBegin();

// Appeler dans loop() pour maintenir le pont actif
void serialBridgeLoop();
