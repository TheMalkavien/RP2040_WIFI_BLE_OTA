#pragma once
#include <StreamString.h>

class Uploader {
    public:
        virtual void Setup() = 0;
        virtual void notifyClients(const String &message) = 0;  
        virtual void loop() = 0;
};

