#include "serial_bridge.h"
#include "config.h"
#include "rp2040_flasher/rp2040_flasher.h"

extern FlasherState flasherState;

#define UART_BUFFER_SIZE 256
static WiFiServer tcpServer(4403);
static WiFiClient client;

static uint8_t  uart_buffer[UART_BUFFER_SIZE];

void serialBridgeBegin() {

  tcpServer.begin();
  tcpServer.setNoDelay(true);
}

void serialBridgeLoop() {
  if (flasherState != IDLE) {
    return; // Ne pas faire le pont série pendant le flashage
  }
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
    size_t rb = SerialRP2040.read(uart_buffer, sizeof(uart_buffer));
    if (rb && client && client.connected()) client.write(uart_buffer, rb);
  }

  // TCP → UART
  if (client && client.connected()) {
    while (client.available()) {
      size_t rb = client.read((uint8_t*)uart_buffer, sizeof(uart_buffer));
      if (rb) {
        SerialRP2040.write(uart_buffer, rb);
        resetInactivityTimer();
      }
    }
  }
}
