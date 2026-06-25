#ifndef RPI_BRIDGE_H
#define RPI_BRIDGE_H

#include <Arduino.h>
#include <ArduinoJson.h>

// RPi0W Communication via Serial2 (UART2)
// Sends JSON commands, receives JSON responses
// Connect RPi0W UART to CYD GPIO16 (RX) / GPIO17 (TX)

#define RPI_RX_BUF  512
#define RPI_TX_BUF  256
#define RPI_TIMEOUT 3000

enum RPi5GCmd {
    RPI_5G_NONE = 0,
    RPI_5G_MONITOR,
    RPI_5G_BEACON,
    RPI_5G_DEAUTH,
    RPI_5G_PROBE,
    RPI_5G_SCAN,
    RPI_5G_STA_SCAN,
    RPI_5G_AUTH_FLOOD,
    RPI_5G_WASH,
    RPI_5G_REAVER,
    RPI_5G_WARDRIVE,
    RPI_5G_STOP
};

typedef void (*RpiCallback)(const char* type, const char* status, const char* message);

class RpiBridge {
public:
    RpiBridge();
    void begin(int rxPin = 16, int txPin = 17, long baud = 115200);
    void sendCmd(RPi5GCmd cmd, const char* arg1 = nullptr, const char* arg2 = nullptr);
    void setCallback(RpiCallback cb);
    void update();  // call from loop()
    bool isConnected() { return _connected; }
    const char* lastMsg() { return _lastMsg; }
    uint32_t lastCount() { return _lastCount; }

private:
    void processLine(const char* line);
    void sendJson(const char* cmd, const char* arg1, const char* arg2);
    bool _connected;
    char _rxBuf[RPI_RX_BUF];
    size_t _rxLen;
    char _lastMsg[128];
    uint32_t _lastCount;
    JsonDocument _jsonDoc;
    RpiCallback _callback;
};

extern RpiBridge rpiBridge;

#endif
