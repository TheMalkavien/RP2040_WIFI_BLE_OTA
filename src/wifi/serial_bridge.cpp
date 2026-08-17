#include "serial_bridge.h"
#include "config.h"
#include "rp2040_flasher/rp2040_flasher.h"

extern FlasherState flasherState;

#define UART_BUFFER_SIZE 256
static WiFiServer tcpServer(4403);
static WiFiClient client;

static uint8_t  uart_buffer[UART_BUFFER_SIZE];

/* ===== Console série sur WebSocket (page serial.html) =======================
   Même service que le port TCP 4403, mais utilisable depuis un navigateur.
   Les octets UART sont agrégés pendant CONSOLE_FLUSH_MS avant d'être envoyés en
   une seule trame binaire: à 921600 bauds une trame par lecture UART saturerait
   immédiatement la file d'attente du WebSocket.
   Le sens navigateur -> UART passe par un anneau SPSC (producteur: tâche async
   du serveur web, consommateur: loop()) pour que seul loop() écrive sur l'UART
   et n'entre jamais en concurrence avec le flasheur.                        */

#define CONSOLE_TX_SIZE   1024   // UART -> navigateur
#define CONSOLE_RX_SIZE   1024   // navigateur -> UART
#define CONSOLE_FLUSH_MS  15

static AsyncWebSocket consoleWs("/wsserial");
static bool     consoleWsInit   = false;

static uint8_t  console_tx[CONSOLE_TX_SIZE];
static size_t   console_tx_len  = 0;
static uint32_t console_tx_last = 0;
static uint32_t console_dropped = 0;

static uint8_t  console_rx[CONSOLE_RX_SIZE];
static volatile size_t console_rx_head = 0;   // écrit par la tâche async
static volatile size_t console_rx_tail = 0;   // écrit par loop()
static volatile bool   console_rx_full = false;

static uint32_t currentBaud = RP2040_SERIAL_BAUD;
static volatile uint32_t pendingBaud  = 0;
static volatile bool     pendingReset = false;

uint32_t serialConsoleBaud() { return currentBaud; }

bool serialConsoleRequestBaud(uint32_t baud) {
  if (baud < 300 || baud > 3000000) return false;
  pendingBaud = baud;
  return true;
}

static void applyBaud(uint32_t baud) {
  if (baud == currentBaud) return;
  SerialRP2040.updateBaudRate(baud);
  currentBaud = baud;
  DEBUG(printf("[Console] baudrate: %lu\n", (unsigned long)baud));
  consoleWs.textAll(String("INFO:BAUD:") + currentBaud);
}

// Vide le tampon d'émission vers les clients WebSocket.
static void consoleFlush(bool force) {
  if (!console_tx_len) return;
  if (!consoleWs.count()) { console_tx_len = 0; console_dropped = 0; return; }

  uint32_t now = millis();
  if (!force && console_tx_len < (CONSOLE_TX_SIZE / 2)
      && (now - console_tx_last) < CONSOLE_FLUSH_MS) return;

  if (!consoleWs.availableForWriteAll()) return;   // file pleine: on retentera

  consoleWs.binaryAll(console_tx, console_tx_len);
  console_tx_len  = 0;
  console_tx_last = now;

  if (console_dropped) {
    consoleWs.textAll(String("INFO:DROPPED:") + console_dropped);
    console_dropped = 0;
  }
}

// Empile des octets UART pour les clients WebSocket (appelé depuis loop()).
static void consolePush(const uint8_t* data, size_t len) {
  if (!consoleWs.count()) { console_tx_len = 0; return; }
  while (len) {
    size_t room = CONSOLE_TX_SIZE - console_tx_len;
    if (!room) {
      consoleFlush(true);
      room = CONSOLE_TX_SIZE - console_tx_len;
      if (!room) { console_dropped += len; return; }  // navigateur trop lent
    }
    size_t n = (len < room) ? len : room;
    memcpy(console_tx + console_tx_len, data, n);
    console_tx_len += n;
    data += n;
    len  -= n;
  }
}

// Empile des octets reçus du navigateur (appelé depuis la tâche async).
static void consoleQueueToUart(const uint8_t* data, size_t len) {
  size_t head = console_rx_head;
  size_t tail = console_rx_tail;
  for (size_t i = 0; i < len; i++) {
    size_t next = (head + 1) % CONSOLE_RX_SIZE;
    if (next == tail) { console_rx_full = true; break; }
    console_rx[head] = data[i];
    head = next;
  }
  console_rx_head = head;
}

// Écrit sur l'UART ce que le navigateur a envoyé (appelé depuis loop()).
static void consoleDrainToUart() {
  size_t head = console_rx_head;
  size_t tail = console_rx_tail;
  if (head == tail) return;
  while (tail != head) {
    size_t n = (head > tail) ? (head - tail) : (CONSOLE_RX_SIZE - tail);
    SerialRP2040.write(console_rx + tail, n);
    tail = (tail + n) % CONSOLE_RX_SIZE;
  }
  console_rx_tail = tail;
  if (console_rx_full) {
    console_rx_full = false;
    consoleWs.textAll("INFO:ERROR:tampon d'envoi saturé, octets perdus");
  }
  resetInactivityTimer();
}

static void consoleResetRP2040() {
  DEBUG(println("[Console] reset RP2040"));
  consoleWs.textAll("INFO:RESET");
  digitalWrite(RESETRP2040_PIN, LOW);
  delay(100);
  digitalWrite(RESETRP2040_PIN, HIGH);
}

static void consoleHandleCommand(AsyncWebSocketClient* c, const char* cmd) {
  if (strncmp(cmd, "CMD:BAUD:", 9) == 0) {
    if (!serialConsoleRequestBaud(strtoul(cmd + 9, nullptr, 10)))
      c->text("INFO:ERROR:baudrate invalide");
  } else if (strcmp(cmd, "CMD:RESET_RP2040") == 0) {
    pendingReset = true;
  } else if (strcmp(cmd, "CMD:PING") == 0) {
    c->text("INFO:PONG");
  } else {
    c->text("INFO:ERROR:commande inconnue");
  }
}

static void onConsoleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* c,
                             AwsEventType type, void* arg, uint8_t* data, size_t len) {
  resetInactivityTimer();
  switch (type) {
    case WS_EVT_CONNECT:
      // Sur une console, mieux vaut perdre des octets que fermer la connexion.
      c->setCloseClientOnQueueFull(false);
      DEBUG(printf("[Console] client #%u connecté\n", c->id()));
      c->text(String("INFO:BAUD:") + currentBaud);
      break;

    case WS_EVT_DISCONNECT:
      DEBUG(printf("[Console] client #%u déconnecté\n", c->id()));
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->opcode == WS_TEXT) {
        // Trame texte = commande de contrôle (toujours courte, non fragmentée)
        if (!info->final || info->index != 0 || info->len != len) break;
        char buf[64];
        size_t n = (len < sizeof(buf) - 1) ? len : sizeof(buf) - 1;
        memcpy(buf, data, n);
        buf[n] = 0;
        consoleHandleCommand(c, buf);
      } else if (len) {
        // Trame binaire = octets bruts pour le RP2040 (fragments acceptés)
        consoleQueueToUart(data, len);
      }
      break;
    }

    default:
      break;
  }
}

AsyncWebSocket* serialConsoleWs() {
  if (!consoleWsInit) {
    consoleWs.onEvent(onConsoleWsEvent);
    consoleWsInit = true;
  }
  return &consoleWs;
}

void serialBridgeBegin() {

  tcpServer.begin();
  tcpServer.setNoDelay(true);
}

void serialBridgeLoop() {
  if (flasherState != IDLE) {
    // Le flasheur exige le baudrate nominal et un accès exclusif à l'UART
    pendingBaud  = 0;
    pendingReset = false;
    applyBaud(RP2040_SERIAL_BAUD);
    console_tx_len  = 0;
    console_rx_tail = console_rx_head;
    return; // Ne pas faire le pont série pendant le flashage
  }

  consoleWs.cleanupClients();
  if (pendingBaud)  { applyBaud(pendingBaud); pendingBaud = 0; }
  if (pendingReset) { pendingReset = false; consoleResetRP2040(); }

  // Accept new connection
  if (!client || !client.connected()) {
    client = tcpServer.accept();
    if (client) {
      client.setNoDelay(true);
      DEBUG(println("[TCPSerial] client connecté"));
      resetInactivityTimer();
    }
  }

  // UART → TCP + WebSocket
  while (SerialRP2040.available()) {
    size_t rb = SerialRP2040.read(uart_buffer, sizeof(uart_buffer));
    if (!rb) break;
    if (client && client.connected()) client.write(uart_buffer, rb);
    consolePush(uart_buffer, rb);
  }
  consoleFlush(false);

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

  // WebSocket → UART
  consoleDrainToUart();
}
