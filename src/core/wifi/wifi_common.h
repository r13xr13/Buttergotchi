#include "core/display.h"
#include <NTPClient.h>
#include <WiFi.h>

#ifndef __WIFI_COMMON_H__
#define __WIFI_COMMON_H__
// TODO wrap in a class

// Public flags
/**
 * @brief Set true while the radio is in promiscuous/sniffer mode.
 * Event handlers check this flag to avoid calling WiFi APIs that would
 * disrupt raw frame injection.
 */
extern volatile bool wifiPromiscuousActive;

// public
/**
 * @brief disconnects and turns off wifi module
 */
void wifiDisconnect();

/**
 * @brief Opens a menu to connect to a wifi
 * @param mode connection mode(AP, STA, AP_STA)
 * @note This is the primary entry point for establishing connections
 * @note returns false if wifi is already connected
 */
bool wifiConnectMenu(wifi_mode_t = WIFI_MODE_STA);

/**
 * @brief Scans the networks and tries to connect to a known network
 * @param mode connection mode(void)
 * @note This is the primary entry point for establishing connections in the Headless environment
 * @note returns true if connected successfully
 */
bool wifiConnecttoKnownNet(void);

/**
 * @brief returns MAC adress
 */
String checkMAC();

/**
 * @brief tries to connect to min(found_networks, maxSearch) networks
 * using stored passwords
 * @TODO fix: rn it skips open networks due to password == "" check
 */
void wifiConnectTask(void *pvParameters);

/**
 * @brief Ensures esp_netif and the default event loop are initialized (idempotent)
 */
void ensureWifiPlatform();

/**
 * @brief Register WiFi event handlers for reactive disconnect/reconnect
 */
void setupWifiEventHandlers();

// private
/**
 * @brief Connects to wifiNetwork
 */
bool _wifiConnect(const String &ssid, int encryption);
bool _connectToWifiNetwork(const String &ssid, const String &pwd);

/**
 * @brief sets up wifi in AP mode
 * @note wifi.mode should be set before calling the method
 */
bool _setupAP();

void updateTimezoneTask(void *pvParameters);

#endif
