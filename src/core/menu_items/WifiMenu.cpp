#include "WifiMenu.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/webInterface.h"
#include "core/wifi/wg.h"
#include "core/wifi/wifi_common.h"
#include "core/wifi/wifi_mac.h"
#include "modules/wifi/sniffer.h"
#include "modules/wifi/eapol_export.h"
#include "modules/wifi/wifi_atks.h"

// global toggle - controls whether scanNetworks includes hidden SSIDs
bool showHiddenNetworks = false;

static void displayAPInfo() {
    displayInfo(
        "SSID: " + WiFi.SSID() + "\nIP: " + WiFi.localIP().toString() +
        "\nPwd: " + buttergotchiConfig.wifiAp.pwd,
        true
    );
}

void WifiMenu::optionsMenu() {
    returnToMenu = false;
    options.clear();
    // Note: WiFi features will cleanly stop WebUI automatically when they start
    // User can navigate menu normally even with WebUI active
    if (WiFi.status() != WL_CONNECTED) {
        options = {
            {"Connect to Wifi", lambdaHelper(wifiConnectMenu, WIFI_STA)},
            {"Start WiFi AP", [=]() {
                 wifiConnectMenu(WIFI_AP);
                 displayInfo("pwd: " + buttergotchiConfig.wifiAp.pwd, true);
             }},
        };
    }
    if (WiFi.getMode() != WIFI_MODE_NULL) { options.push_back({"Turn Off WiFi", wifiDisconnect}); }
    if (WiFi.getMode() == WIFI_MODE_STA || WiFi.getMode() == WIFI_MODE_APSTA) {
        options.push_back({"AP info", displayAPInfo});
    }
    options.push_back({"Wifi Atks", wifi_atk_menu});
    options.push_back({"Sniffer", sniffer_setup});
    options.push_back({"EAPOL Export", eapol_export_menu});

    options.push_back({"Config", [this]() { configMenu(); }});

    addOptionToMainMenu();

    loopOptions(options, MENU_TYPE_SUBMENU, "WiFi");

    options.clear();
}

void WifiMenu::configMenu() {
    std::vector<Option> wifiOptions;

    wifiOptions.push_back({"Change MAC", wifiMACMenu});
    wifiOptions.push_back({"Back", [this]() { optionsMenu(); }});
    loopOptions(wifiOptions, MENU_TYPE_SUBMENU, "WiFi Config");
}

void WifiMenu::drawIcon(float scale) {
    clearIconArea();
    int deltaY = scale * 20;
    int radius = scale * 6;

    tft.fillCircle(iconCenterX, iconCenterY + deltaY, radius, buttergotchiConfig.priColor);
    tft.drawArc(
        iconCenterX,
        iconCenterY + deltaY,
        deltaY + radius,
        deltaY,
        130,
        230,
        buttergotchiConfig.priColor,
        buttergotchiConfig.bgColor
    );
    tft.drawArc(
        iconCenterX,
        iconCenterY + deltaY,
        2 * deltaY + radius,
        2 * deltaY,
        130,
        230,
        buttergotchiConfig.priColor,
        buttergotchiConfig.bgColor
    );
}
