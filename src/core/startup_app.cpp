/**
 * @file startup_app.cpp
 * @author Rennan Cockles (https://github.com/rennancockles)
 * @brief Buttergotchi startup apps
 * @version 0.1
 * @date 2024-11-20
 */

#include "startup_app.h"

#include "core/settings.h" // clock
#include "core/wifi/webInterface.h"
#include "core/wifi/wifi_common.h"
#include "modules/buttergotchi/buttergottchi.h"
#include "modules/wifi/sniffer.h"

StartupApp::StartupApp() {
    _startupApps["Buttergotchi"] = []() { buttergotchi_start(); };
    _startupApps["Sniffer"] = []() { sniffer_setup(); };
    _startupApps["Clock"] = []() { runClockLoop(); };
    _startupApps["WebUI"] = []() { startWebUi(!wifiConnecttoKnownNet()); };
}

bool StartupApp::startApp(const String &appName) const {
    auto it = _startupApps.find(appName);
    if (it == _startupApps.end()) {
        Serial.println("Invalid startup app: " + appName);
        return false;
    }

    it->second();

    delay(200);
    tft.fillScreen(buttergotchiConfig.bgColor);

    return true;
}

std::vector<String> StartupApp::getAppNames() const {
    std::vector<String> keys;
    for (const auto &pair : _startupApps) { keys.push_back(pair.first); }
    return keys;
}
