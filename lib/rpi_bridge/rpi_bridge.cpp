#include "rpi_bridge.h"

RpiBridge rpiBridge;

RpiBridge::RpiBridge() : _connected(false), _rxLen(0), _lastCount(0), _callback(nullptr) {
    _lastMsg[0] = '\0';
}

void RpiBridge::begin(int rxPin, int txPin, long baud) {
    Serial2.begin(baud, SERIAL_8N1, rxPin, txPin);
    delay(100);
    _connected = true;
    _rxLen = 0;
    Serial.println("[RPI] Bridge started on Serial2");
}

void RpiBridge::sendJson(const char* cmd, const char* arg1, const char* arg2) {
    if (!_connected) return;
    _jsonDoc.clear();
    _jsonDoc["cmd"] = cmd;
    _jsonDoc["ts"] = millis();
    if (arg1) _jsonDoc["arg1"] = arg1;
    if (arg2) _jsonDoc["arg2"] = arg2;
    String msg;
    serializeJson(_jsonDoc, msg);
    Serial2.println(msg);
    Serial.printf("[RPI→] %s\n", msg.c_str());
}

void RpiBridge::sendCmd(RPi5GCmd cmd, const char* arg1, const char* arg2) {
    switch (cmd) {
        case RPI_5G_MONITOR:    sendJson("5g_monitor", arg1, arg2); break;
        case RPI_5G_BEACON:     sendJson("5g_beacon", arg1, arg2); break;
        case RPI_5G_DEAUTH:     sendJson("5g_deauth", arg1, arg2); break;
        case RPI_5G_PROBE:      sendJson("5g_probe", arg1, arg2); break;
        case RPI_5G_SCAN:       sendJson("5g_scan", arg1, arg2); break;
        case RPI_5G_STA_SCAN:   sendJson("5g_sta_scan", arg1, arg2); break;
        case RPI_5G_AUTH_FLOOD: sendJson("5g_auth_flood", arg1, arg2); break;
        case RPI_5G_WASH:       sendJson("wash", arg1, arg2); break;
        case RPI_5G_WARDRIVE:   sendJson("wardrive_5g", arg1, arg2); break;
        case RPI_5G_REAVER:     sendJson("reaver", arg1, arg2); break;
        case RPI_5G_STOP:       sendJson("stop", arg1, arg2); break;
        default: break;
    }
}

void RpiBridge::processLine(const char* line) {
    // Parse JSON response from RPi
    // Format: {"type":"scan","status":"ok","message":"5 APs found","count":5}
    _jsonDoc.clear();
    DeserializationError err = deserializeJson(_jsonDoc, line);
    if (err) { Serial.printf("[RPI] JSON err: %s\n", err.c_str()); return; }

    const char* type = _jsonDoc["type"] | "";
    const char* status = _jsonDoc["status"] | "";
    const char* msg = _jsonDoc["message"] | "";
    uint32_t count = _jsonDoc["count"] | 0;

    strncpy(_lastMsg, msg, sizeof(_lastMsg) - 1);
    _lastCount = count;

    Serial.printf("[RPI←] %s: %s (%s)\n", type, status, msg);

    if (_callback) _callback(type, status, msg);
}

void RpiBridge::setCallback(RpiCallback cb) { _callback = cb; }

void RpiBridge::update() {
    if (!_connected) return;
    while (Serial2.available() > 0) {
        char c = Serial2.read();
        if (c == '\n' || c == '\r') {
            if (_rxLen > 0) {
                _rxBuf[_rxLen] = '\0';
                if (_rxBuf[0] == '{') processLine(_rxBuf);
                _rxLen = 0;
            }
        } else {
            if (_rxLen < RPI_RX_BUF - 1) _rxBuf[_rxLen++] = c;
        }
    }
}
