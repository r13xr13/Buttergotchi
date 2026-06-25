#include "core/wifi/wifi_common.h"
#include "core/display.h"    // using displayRedStripe  and loop options
#include "core/mykeyboard.h" // usinf keyboard when calling rename
#include "core/powerSave.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/wifi_mac.h" // Set Mac Address - @IncursioHack
// #include "modules/ble/ble_common.h" (module removed)
#include <esp_event.h>
#include <esp_netif.h>
#include <globals.h>

static TaskHandle_t timezoneTaskHandle = NULL;
static bool wifiTransitioning = false;
static bool wifiEventHandlersRegistered = false;

volatile bool wifiPromiscuousActive = false;

// Forward declaration
static void wifiReconnectTask(void *pvParameters);

void setupWifiEventHandlers() {
    if (wifiEventHandlersRegistered) return;
    wifiEventHandlersRegistered = true;

    WiFi.onEvent([](WiFiEvent_t event, arduino_event_info_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            // Skip if in promiscuous (sniffer / pwnagotchi) mode —
            // calling WiFi APIs from handlers can corrupt the radio state
            // and cause raw frame injection to fail or crash the stack.
            if (wifiPromiscuousActive) return;
            wifiConnected = false;
            xTaskCreate(wifiReconnectTask, "wifiReconnect", 4096, NULL, 1, NULL);
        }
    });

    WiFi.onEvent([](WiFiEvent_t event, arduino_event_info_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            if (wifiPromiscuousActive) return;
            wifiConnected = true;
        }
    });
}

static void wifiReconnectTask(void *pvParameters) {
    if (wifiTransitioning) {
        vTaskDelete(NULL);
        return;
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    if (WiFi.status() != WL_CONNECTED) {
        wifiConnecttoKnownNet();
    }

    vTaskDelete(NULL);
}

void ensureWifiPlatform() {
    static bool netifInitialized = false;
    static bool eventLoopCreated = false;
    static portMUX_TYPE platformMux = portMUX_INITIALIZER_UNLOCKED;

    portENTER_CRITICAL(&platformMux);
    bool needNetif = !netifInitialized;
    bool needLoop = !eventLoopCreated;
    portEXIT_CRITICAL(&platformMux);

    if (needNetif) {
        ESP_ERROR_CHECK(esp_netif_init());
        portENTER_CRITICAL(&platformMux);
        netifInitialized = true;
        portEXIT_CRITICAL(&platformMux);
    }

    if (needLoop) {
        esp_err_t err = esp_event_loop_create_default();
        if (err != ESP_ERR_INVALID_STATE) { ESP_ERROR_CHECK(err); }
        portENTER_CRITICAL(&platformMux);
        eventLoopCreated = true;
        portEXIT_CRITICAL(&platformMux);
    }

    // Register WiFi event handlers once (safe even before WiFi.mode())
    setupWifiEventHandlers();
}

// Heuristic: detect old encrypted config data (hex:hex format from removed
// HEAVYBUTTER_HARDENED layer). If the stored password looks like stale
// crypto garbage, treat it as missing so the user is prompted for a real one.
static bool looksLikeOldEncryptedData(const String &s) {
    if (s.length() < 40) return false;              // too short to be encrypted
    int colon = s.indexOf(':');
    if (colon < 0) return false;                    // no colon separator
    // Check that chars before colon are all hex digits
    for (int i = 0; i < colon; i++) {
        if (!isxdigit(s[i])) return false;
    }
    return true;
}

bool _wifiConnect(const String &ssid, int encryption) {
    String password = buttergotchiConfig.getWifiPassword(ssid);
    if (looksLikeOldEncryptedData(password)) password = "";  // stale encrypted data
    if (password == "" && encryption > 0) { password = keyboard(password, 63, "Network Password:", true); }
    if (password == "\x1B") return false;
    bool connected = _connectToWifiNetwork(ssid, password);
    bool retry = false;

    while (!connected) {
        wakeUpScreen();

        options = {
            {"Retry",  [&]() { retry = true; } },
            {"Cancel", [&]() { retry = false; }},
        };
        loopOptions(options);

        if (!retry) {
            wifiDisconnect();
            return false;
        }

        password = keyboard(password, 63, "Network Password:", true);
        if (password == "\x1B") {
            wifiDisconnect();
            return false;
        }
        connected = _connectToWifiNetwork(ssid, password);
    }

    if (connected) {
        wifiConnected = true;
        wifiIP = WiFi.localIP().toString();
        buttergotchiConfig.addWifiCredential(ssid, password);

        // Start timezone update in background if not already running
        if (timezoneTaskHandle == NULL) {
            xTaskCreate(updateTimezoneTask, "updateTimezone", 4096, NULL, 1, &timezoneTaskHandle);
        }
    }

    // Ensure DNS is configured (DHCP sometimes fails to set it)
    if (connected && WiFi.dnsIP() == INADDR_NONE) {
        IPAddress dns1(8, 8, 8, 8), dns2(8, 8, 4, 4);
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
    }

    delay(200);
    return connected;
}

bool _connectToWifiNetwork(const String &ssid, const String &pwd) {
    // Free any stale scan results that can interfere with WiFi.begin()
    WiFi.scanDelete();

    // Stop BLE to free the radio for WiFi — BLE+WiFi share a single radio on ESP32-S3.
    // This is unconditional: the boot-time BleKeyboard task runs NimBLE on Core 0 and
    // isn't tracked by the BLEConnected flag.
    
    vTaskDelay(300 / portTICK_PERIOD_MS);

    drawMainBorderWithTitle("WiFi Connect");
    padprintln("");
    padprint("Connecting to: " + ssid + ".");
    WiFi.mode(WIFI_MODE_STA);
    WiFi.setSleep(WIFI_PS_NONE);
    WiFi.setHostname("buttergotchi");
    vTaskDelay(10 / portTICK_PERIOD_MS);
    WiFi.begin(ssid, pwd);

    int i = 1;
    while (WiFi.status() != WL_CONNECTED) {
        if (tft.getCursorX() >= tftWidth - 12) {
            padprintln("");
            padprint("");
        }
#ifdef HAS_SCREEN
        tft.print(".");
#else
        Serial.print(".");
#endif

        if (i > 30) {
            displayError("Wifi Offline");
            vTaskDelay(500 / portTICK_RATE_MS);
            break;
        }

        vTaskDelay(500 / portTICK_RATE_MS);
        i++;
    }

    return WiFi.status() == WL_CONNECTED;
}

bool _setupAP() {
    // Clean AP setup — radio management (BLE teardown, WiFi disconnect) is
    // handled by the caller (wifiConnectMenu) before we get here.
    IPAddress AP_GATEWAY(172, 0, 0, 1);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_GATEWAY, AP_GATEWAY, IPAddress(255, 255, 255, 0));
    WiFi.softAP(buttergotchiConfig.wifiAp.ssid, buttergotchiConfig.wifiAp.pwd, 6, 0, 4, false);
    wifiIP = WiFi.softAPIP().toString();
    wifiConnected = true;
    return true;
}

void wifiDisconnect() {
    wifiTransitioning = true;

    WiFi.softAPdisconnect(true); // turn off AP mode
    vTaskDelay(10 / portTICK_PERIOD_MS);
    WiFi.disconnect(true, true); // turn off STA mode
    vTaskDelay(10 / portTICK_PERIOD_MS);
    WiFi.mode(WIFI_OFF); // enforces WIFI_OFF mode
    vTaskDelay(10 / portTICK_PERIOD_MS);

    wifiConnected = false;
    wifiTransitioning = false;
}

bool wifiConnectMenu(wifi_mode_t mode) {
    if (WiFi.isConnected()) return false; // safeguard

    // Stop BLE before any WiFi mode change (radio coexistence).
    // This is unconditional: the boot-time BleKeyboard task runs NimBLE on
    // Core 0 and isn't tracked by the BLEConnected flag.
    
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Check if WiFi is in transition
    if (wifiTransitioning) {
        displayTextLine("WiFi busy, please wait...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        return false;
    }

    switch (mode) {
        case WIFI_AP: // access point — _setupAP() handles mode reset internally
            return _setupAP();
            break;

        case WIFI_STA: { // station mode
            int nets;
            WiFi.mode(WIFI_MODE_STA);

            // wifiMACMenu();
            applyConfiguredMAC();

            bool refresh_scan = false;
            do {
                displayTextLine("Scanning..");
                nets = WiFi.scanNetworks();
                // Collect visible networks with RSSI > -85 dBm and sort by signal strength
                std::vector<int> netIndices;
                for (int i = 0; i < nets; i++) {
                    if (WiFi.RSSI(i) > -85) netIndices.push_back(i);
                }
                std::sort(netIndices.begin(), netIndices.end(), [](int a, int b) {
                    return WiFi.RSSI(a) > WiFi.RSSI(b);
                });
                options = {};
                for (int idx : netIndices) {
                    if (options.size() >= 250) break;
                    String ssid = WiFi.SSID(idx);
                    int encryptionType = WiFi.encryptionType(idx);
                    int32_t rssi = WiFi.RSSI(idx);
                    int32_t ch = WiFi.channel(idx);
                    String encryptionPrefix = (encryptionType == WIFI_AUTH_OPEN) ? "" : "#";
                    String encryptionTypeStr;
                    switch (encryptionType) {
                        case WIFI_AUTH_OPEN: encryptionTypeStr = "Open"; break;
                        case WIFI_AUTH_WEP: encryptionTypeStr = "WEP"; break;
                        case WIFI_AUTH_WPA_PSK: encryptionTypeStr = "WPA/PSK"; break;
                        case WIFI_AUTH_WPA2_PSK: encryptionTypeStr = "WPA2/PSK"; break;
                        case WIFI_AUTH_WPA_WPA2_PSK: encryptionTypeStr = "WPA/WPA2/PSK"; break;
                        case WIFI_AUTH_WPA2_ENTERPRISE: encryptionTypeStr = "WPA2/Enterprise"; break;
                        default: encryptionTypeStr = "Unknown"; break;
                    }

                    String optionText = encryptionPrefix + ssid + "(" + String(rssi) + "|" +
                                        encryptionTypeStr + "|ch." + String(ch) + ")";

                    options.push_back({optionText.c_str(), [=]() {
                                           _wifiConnect(ssid, encryptionType);
                                       }});
                }
                options.push_back({"Hidden SSID", [=]() {
                                       String __ssid = keyboard("", 32, "Your SSID");
                                       if (__ssid != "\x1B") _wifiConnect(__ssid.c_str(), 8);
                                   }});
                addOptionToMainMenu();

                loopOptions(options);
                options.clear();

                if (check(EscPress)) {
                    refresh_scan = true;
                } else {
                    refresh_scan = false;
                }
            } while (refresh_scan);
            WiFi.scanDelete();  // free scan results after user selection
        } break;

        case WIFI_AP_STA: // repeater mode
                          // _setupRepeater();
            break;

        default: // error handling
            Serial.println("Unknown wifi mode: " + String(mode));
            break;
    }

    if (returnToMenu) {
        wifiDisconnect(); // Forced turning off the wifi module if exiting back to the menu
        return false;
    }
    return wifiConnected;
}

void wifiConnectTask(void *pvParameters) {
    if (WiFi.status() == WL_CONNECTED) return;

    // Stop BLE to free the radio for WiFi
    if (BLEConnected) {
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Check if WiFi is in transition
    if (wifiTransitioning) {
        vTaskDelete(NULL);
        return;
    }

    WiFi.mode(WIFI_MODE_STA);
    WiFi.setSleep(WIFI_PS_NONE);
    int nets = WiFi.scanNetworks();
    String ssid;
    String pwd;

    for (int i = 0; i < nets; i++) {
        ssid = WiFi.SSID(i);
        pwd = buttergotchiConfig.getWifiPassword(ssid);
        if (pwd == "") continue;

        WiFi.setHostname("buttergotchi");
        WiFi.begin(ssid, pwd);
        for (int i = 0; i < 50; i++) {
            if (WiFi.status() == WL_CONNECTED) {
                wifiConnected = true;
                wifiIP = WiFi.localIP().toString();

                // Start timezone update in background if not already running
                if (timezoneTaskHandle == NULL) {
                    xTaskCreate(updateTimezoneTask, "updateTimezone", 4096, NULL, 1, &timezoneTaskHandle);
                }
                drawStatusBar();
                break;
            }
            vTaskDelay(100 / portTICK_RATE_MS);
        }
    }
    WiFi.scanDelete();

    vTaskDelete(NULL);
    return;
}

String checkMAC() { return String(WiFi.macAddress()); }

bool wifiConnecttoKnownNet(void) {
    if (WiFi.isConnected()) return true; // safeguard

    // Stop BLE to free the radio for WiFi — unconditional (BleKeyboard on Core 0
    // isn't tracked by BLEConnected flag).
    
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Check if WiFi is in transition
    if (wifiTransitioning) {
        displayTextLine("WiFi busy, please wait...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        return false;
    }

    bool result = false;
    int nets;
    displayTextLine("Scanning Networks..");
    WiFi.disconnect(true, true);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // Retry loop: up to 3 full scan+connect attempts with backoff
    for (int attempt = 0; attempt < 3 && !result; attempt++) {
        if (attempt > 0) {
            displayTextLine("Retrying scan...");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        nets = WiFi.scanNetworks();
        // Sort by RSSI so we try strongest signals first
        std::vector<int> netIndices;
        for (int i = 0; i < nets; i++) netIndices.push_back(i);
        std::sort(netIndices.begin(), netIndices.end(), [](int a, int b) {
            return WiFi.RSSI(a) > WiFi.RSSI(b);
        });
        for (int idx : netIndices) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            String ssid = WiFi.SSID(idx);
            String password = buttergotchiConfig.getWifiPassword(ssid);
            if (password != "") {
                Serial.println("Connecting to: " + ssid);
                result = _connectToWifiNetwork(ssid, password);
            }
            if (result) {
                Serial.println("Connected to: " + ssid);
                break;
            }
        }
    }
    WiFi.scanDelete();
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        wifiIP = WiFi.localIP().toString();

        // Start timezone update in background if not already running
        if (timezoneTaskHandle == NULL) {
            xTaskCreate(updateTimezoneTask, "updateTimezone", 4096, NULL, 1, &timezoneTaskHandle);
        }
    }
    return result;
}

void updateTimezoneTask(void *pvParameters) {
    // Wait a bit for connection to stabilize before updating timezone
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // Only update timezone if WiFi is still connected
    if (WiFi.isConnected() && wifiConnected) { updateClockTimezone(); }

    // Clear the task handle before deleting
    timezoneTaskHandle = NULL;
    vTaskDelete(NULL);
}
