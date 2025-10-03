#pragma once

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Arduino.h>
#include <LittleFS.h>
#include "uploader.h"
#include "config.h"


class WifiUpload : public Uploader {
  public:   
    WifiUpload();
    ~WifiUpload();  
    void Setup();
    void notifyClients(const String &message);  
    void loop();
    static void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    private:    
    AsyncWebServer *server;
    AsyncWebSocket *ws;
};
    
int onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

extern Uploader* uploader;