#include "serial_bridge.h"
#include "config.h"
#include "rp2040_flasher/rp2040_flasher.h"

static WiFiServer tcpServer(4403);
static WiFiClient client;

void serialBridgeBegin() {

  tcpServer.begin();
  tcpServer.setNoDelay(true);
}

void serialBridgeLoop() {
  // Accept new connection
  if (!client || !client.connected()) {
    client = tcpServer.accept();
    if (client) {
      client.setNoDelay(true);
      DEBUG(println("[TCPSerial] client connecté"));
      resetInactivityTimer();
    }
  }

  // UART → TCP
  while (SerialRP2040.available()) {
    uint8_t b = SerialRP2040.read();
    if (client && client.connected()) client.write(&b, 1);
  }

  // TCP → UART
  if (client && client.connected()) {
    while (client.available()) {
      uint8_t b = client.read();
      SerialRP2040.write(b);
      resetInactivityTimer();
    }
  }
}
