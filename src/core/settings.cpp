#include "settings.h"
#include "core/led_control.h"
#include "core/wifi/wifi_common.h"
#include "current_year.h"
#include "display.h"

#include "mykeyboard.h"
#include "powerSave.h"
#include "sd_functions.h"
#include "settingsColor.h"
#include "utils.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <globals.h>
#include <mbedtls/sha256.h>

static bool verify_sha256(const uint8_t *data, size_t len, const char *expected_hex) {
    if (expected_hex == NULL || strlen(expected_hex) == 0) {
        return true;
    }
    unsigned char hash[32];
    if (mbedtls_sha256(data, len, hash, 0) != 0) {
        return false;
    }
    char hex[65];
    for (int i = 0; i < 32; i++) {
        snprintf(hex + i * 2, sizeof(hex) - i * 2, "%02x", hash[i]);
    }
    hex[64] = '\0';
    return strcmp(hex, expected_hex) == 0;
}


int currentScreenBrightness = -1;

// This function comes from interface.h
void _setBrightness(uint8_t brightval) {}

/*********************************************************************
**  Function: setBrightness
**  set brightness value
**********************************************************************/
void setBrightness(uint8_t brightval, bool save) {
    if (buttergotchiConfig.bright > 100) buttergotchiConfig.setBright(100);
    _setBrightness(brightval);
    delay(10);

    currentScreenBrightness = brightval;
    if (save) { buttergotchiConfig.setBright(brightval); }
}

/*********************************************************************
**  Function: getBrightness
**  get brightness value
**********************************************************************/
void getBrightness() {
    if (buttergotchiConfig.bright > 100) {
        buttergotchiConfig.setBright(100);
        _setBrightness(buttergotchiConfig.bright);
        delay(10);
        setBrightness(100);
    }

    _setBrightness(buttergotchiConfig.bright);
    delay(10);

    currentScreenBrightness = buttergotchiConfig.bright;
}

/*********************************************************************
**  Function: gsetRotation
**  get/set rotation value
**********************************************************************/
int gsetRotation(bool set) {
    int getRot = buttergotchiConfigPins.rotation;
    int result = ROTATION;
    int mask = ROTATION > 1 ? -2 : 2;

    options = {
        {"Default",         [&]() { result = ROTATION; }                        },
        {"Landscape (180)", [&]() { result = ROTATION + mask; }                 },
#if TFT_WIDTH >= 170 && TFT_HEIGHT >= 240
        {"Portrait (+90)",  [&]() { result = ROTATION > 0 ? ROTATION - 1 : 3; } },
        {"Portrait (-90)",  [&]() { result = ROTATION == 3 ? 0 : ROTATION + 1; }},

#endif
    };
    addOptionToMainMenu();
    if (set) loopOptions(options);
    else result = getRot;

    if (result > 3 || result < 0) {
        result = ROTATION;
        set = true;
    }
    if (set) {
        buttergotchiConfigPins.setRotation(result);
        tft.setRotation(result);
        tft.setRotation(result); // must repeat, sometimes ESP32S3 miss one SPI command and it just
                                 // jumps this step and don't rotate
    }
    returnToMenu = true;

    if (result & 0b01) { // if 1 or 3
        tftWidth = TFT_HEIGHT;
#if defined(HAS_TOUCH)
        tftHeight = TFT_WIDTH - 20;
#else
        tftHeight = TFT_WIDTH;
#endif
    } else { // if 2 or 0
        tftWidth = TFT_WIDTH;
#if defined(HAS_TOUCH)
        tftHeight = TFT_HEIGHT - 20;
#else
        tftHeight = TFT_HEIGHT;
#endif
    }
    return result;
}

/*********************************************************************
**  Function: setBrightnessMenu
**  Handles Menu to set brightness
**********************************************************************/
void setBrightnessMenu() {
    int idx = 0;
    if (buttergotchiConfig.bright == 100) idx = 0;
    else if (buttergotchiConfig.bright == 75) idx = 1;
    else if (buttergotchiConfig.bright == 50) idx = 2;
    else if (buttergotchiConfig.bright == 25) idx = 3;
    else if (buttergotchiConfig.bright == 1) idx = 4;

    options = {
        {"100%",
         [=]() { setBrightness((uint8_t)100); },
         buttergotchiConfig.bright == 100,
         [](void *pointer, bool shouldRender) {
             setBrightness((uint8_t)100, false);
             return false;
         }},
        {"75 %",
         [=]() { setBrightness((uint8_t)75); },
         buttergotchiConfig.bright == 75,
         [](void *pointer, bool shouldRender) {
             setBrightness((uint8_t)75, false);
             return false;
         }},
        {"50 %",
         [=]() { setBrightness((uint8_t)50); },
         buttergotchiConfig.bright == 50,
         [](void *pointer, bool shouldRender) {
             setBrightness((uint8_t)50, false);
             return false;
         }},
        {"25 %",
         [=]() { setBrightness((uint8_t)25); },
         buttergotchiConfig.bright == 25,
         [](void *pointer, bool shouldRender) {
             setBrightness((uint8_t)25, false);
             return false;
         }},
        {" 1 %",
         [=]() { setBrightness((uint8_t)1); },
         buttergotchiConfig.bright == 1,
         [](void *pointer, bool shouldRender) {
             setBrightness((uint8_t)1, false);
             return false;
         }}
    };
    addOptionToMainMenu(); // this one bugs the brightness selection
    loopOptions(options, MENU_TYPE_REGULAR, "", idx);
    setBrightness(buttergotchiConfig.bright, false);
}

/*********************************************************************
**  Function: setSleepMode
**  Turn screen off and reduces cpu clock
**********************************************************************/
void setSleepMode() {
    // Power down radios during sleep mode
    WiFi.mode(WIFI_OFF);
    btStop();
    sleepModeOn();
    while (1) {
        if (check(AnyKeyPress)) {
            sleepModeOff();
            returnToMenu = true;
            break;
        }
    }
}

/*********************************************************************
**  Function: setDimmerTimeMenu
**  Handles Menu to set dimmer time
**********************************************************************/
void setDimmerTimeMenu() {
    int idx = 0;
    if (buttergotchiConfig.dimmerSet == 10) idx = 0;
    else if (buttergotchiConfig.dimmerSet == 20) idx = 1;
    else if (buttergotchiConfig.dimmerSet == 30) idx = 2;
    else if (buttergotchiConfig.dimmerSet == 60) idx = 3;
    else if (buttergotchiConfig.dimmerSet == 0) idx = 4;
    options = {
        {"10s",      [=]() { buttergotchiConfig.setDimmer(10); }, buttergotchiConfig.dimmerSet == 10},
        {"20s",      [=]() { buttergotchiConfig.setDimmer(20); }, buttergotchiConfig.dimmerSet == 20},
        {"30s",      [=]() { buttergotchiConfig.setDimmer(30); }, buttergotchiConfig.dimmerSet == 30},
        {"60s",      [=]() { buttergotchiConfig.setDimmer(60); }, buttergotchiConfig.dimmerSet == 60},
        {"Disabled", [=]() { buttergotchiConfig.setDimmer(0); },  buttergotchiConfig.dimmerSet == 0 },
    };
    loopOptions(options, idx);
}

/*********************************************************************
**  Function: setUIColor
**  Set and store main UI color
**********************************************************************/
void setUIColor() {

    while (1) {
        options.clear();
        int idx = UI_COLOR_COUNT;
        int i = 0;
        for (const auto &mapping : UI_COLORS) {
            if (buttergotchiConfig.priColor == mapping.priColor && buttergotchiConfig.secColor == mapping.secColor &&
                buttergotchiConfig.bgColor == mapping.bgColor) {
                idx = i;
            }

            options.emplace_back(
                mapping.name,
                [=, &mapping]() {
                    uint16_t secColor = mapping.secColor;
                    uint16_t bgColor = mapping.bgColor;
                    buttergotchiConfig.setUiColor(mapping.priColor, &secColor, &bgColor);
                },
                idx == i
            );
            ++i;
        }

        options.push_back(
            {"Custom Color",
             [=]() {
                 uint16_t oldPriColor = buttergotchiConfig.priColor;
                 uint16_t oldSecColor = buttergotchiConfig.secColor;
                 uint16_t oldBgColor = buttergotchiConfig.bgColor;

                 if (setCustomUIColorMenu()) {
                     buttergotchiConfig.setUiColor(
                         buttergotchiConfig.priColor, &buttergotchiConfig.secColor, &buttergotchiConfig.bgColor
                     );
                 } else {
                     buttergotchiConfig.priColor = oldPriColor;
                     buttergotchiConfig.secColor = oldSecColor;
                     buttergotchiConfig.bgColor = oldBgColor;
                 }
                 tft.setTextColor(buttergotchiConfig.priColor, buttergotchiConfig.bgColor);
             },
             idx == UI_COLOR_COUNT}
        );

        options.push_back(
            {"Invert Color",
             [=]() {
                 buttergotchiConfig.setColorInverted(!buttergotchiConfig.colorInverted);
                 tft.invertDisplay(buttergotchiConfig.colorInverted);
             },
             buttergotchiConfig.colorInverted > 0}
        );

        addOptionToMainMenu();

        int selectedOption = loopOptions(options, idx);
        if (selectedOption == -1 || selectedOption == options.size() - 1) return;
    }
}

uint16_t alterOneColorChannel565(uint16_t color, int newR, int newG, int newB) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;

    if (newR != 256) r = newR & 0x1F;
    if (newG != 256) g = newG & 0x3F;
    if (newB != 256) b = newB & 0x1F;

    return (r << 11) | (g << 5) | b;
}

bool setCustomUIColorMenu() {
    while (1) {
        options = {
            {"Primary",    [=]() { setCustomUIColorChoiceMenu(1); }},
            {"Secondary",  [=]() { setCustomUIColorChoiceMenu(2); }},
            {"Background", [=]() { setCustomUIColorChoiceMenu(3); }},
            {"Save",       [=]() {}                                },
            {"Cancel",     [=]() {}                                }
        };

        int selectedOption = loopOptions(options);
        if (selectedOption == -1 || selectedOption == options.size() - 1) {
            return false;
        } else if (selectedOption == 3) {
            return true;
        }
    }
}

void setCustomUIColorChoiceMenu(int colorType) {
    while (1) {
        options = {
            {"Red Channel",   [=]() { setCustomUIColorSettingMenuR(colorType); }},
            {"Green Channel", [=]() { setCustomUIColorSettingMenuG(colorType); }},
            {"Blue Channel",  [=]() { setCustomUIColorSettingMenuB(colorType); }},
            {"Back",          [=]() {}                                          }
        };

        int selectedOption = loopOptions(options);
        if (selectedOption == -1 || selectedOption == options.size() - 1) return;
    }
}

void setCustomUIColorSettingMenuR(int colorType) {
    setCustomUIColorSettingMenu(colorType, 1, [](uint16_t baseColor, int i) {
        return alterOneColorChannel565(baseColor, i, 256, 256);
    });
}

void setCustomUIColorSettingMenuG(int colorType) {
    setCustomUIColorSettingMenu(colorType, 2, [](uint16_t baseColor, int i) {
        return alterOneColorChannel565(baseColor, 256, i, 256);
    });
}

void setCustomUIColorSettingMenuB(int colorType) {
    setCustomUIColorSettingMenu(colorType, 3, [](uint16_t baseColor, int i) {
        return alterOneColorChannel565(baseColor, 256, 256, i);
    });
}

constexpr const char *colorTypes[] = {
    "Background", // 0
    "Primary",    // 1
    "Secondary"   // 2
};

constexpr const char *rgbNames[] = {
    "Blue", // 0
    "Red",  // 1
    "Green" // 2
};

void setCustomUIColorSettingMenu(
    int colorType, int rgb, std::function<uint16_t(uint16_t, int)> colorGenerator
) {
    uint16_t color = (colorType == 1)   ? buttergotchiConfig.priColor
                     : (colorType == 2) ? buttergotchiConfig.secColor
                                        : buttergotchiConfig.bgColor;

    options.clear();

    static auto hoverFunctionPriColor = [](void *pointer, bool shouldRender) -> bool {
        uint16_t colorToSet = *static_cast<uint16_t *>(pointer);
        // Serial.printf("Setting primary color to: %04X\n", colorToSet);
        buttergotchiConfig.priColor = colorToSet;
        return false;
    };
    static auto hoverFunctionSecColor = [](void *pointer, bool shouldRender) -> bool {
        uint16_t colorToSet = *static_cast<uint16_t *>(pointer);
        // Serial.printf("Setting secondary color to: %04X\n", colorToSet);
        buttergotchiConfig.secColor = colorToSet;
        return false;
    };

    static auto hoverFunctionBgColor = [](void *pointer, bool shouldRender) -> bool {
        uint16_t colorToSet = *static_cast<uint16_t *>(pointer);
        // Serial.printf("Setting bg color to: %04X\n", colorToSet);
        buttergotchiConfig.bgColor = colorToSet;
        tft.fillScreen(buttergotchiConfig.bgColor);
        return false;
    };

    static uint16_t colorStorage[32];
    int selectedIndex = 0;
    int i = 0;
    int index = 0;

    if (rgb == 1) {
        selectedIndex = (color >> 11) & 0x1F;
    } else if (rgb == 2) {
        selectedIndex = ((color >> 5) & 0x3F);
    } else {
        selectedIndex = color & 0x1F;
    }

    while (i <= (rgb == 2 ? 63 : 31)) {
        if (i == 0 || (rgb == 2 && (i + 1) % 2 == 0) || (rgb != 2)) {
            uint16_t updatedColor = colorGenerator(color, i);
            colorStorage[index] = updatedColor;

            options.emplace_back(
                String(i),
                [colorType, updatedColor]() {
                    if (colorType == 1) buttergotchiConfig.priColor = updatedColor;
                    else if (colorType == 2) buttergotchiConfig.secColor = updatedColor;
                    else buttergotchiConfig.bgColor = updatedColor;
                },
                selectedIndex == i,
                (colorType == 1 ? hoverFunctionPriColor
                                : (colorType == 2 ? hoverFunctionSecColor : hoverFunctionBgColor)),
                &colorStorage[index]
            );
            ++index;
        }
        ++i;
    }

    addOptionToMainMenu();

    int selectedOption = loopOptions(
        options,
        MENU_TYPE_SUBMENU,
        (String(colorType == 1 ? "Primary" : (colorType == 2 ? "Secondary" : "Background")) + " - " +
         (rgb == 1 ? "Red" : (rgb == 2 ? "Green" : "Blue")))
            .c_str(),
        (rgb != 2) ? selectedIndex : (selectedIndex > 0 ? (selectedIndex + 1) / 2 : 0)
    );
    if (selectedOption == -1 || selectedOption == options.size() - 1) {
        if (colorType == 1) {
            buttergotchiConfig.priColor = color;
        } else if (colorType == 2) {
            buttergotchiConfig.secColor = color;
        } else {
            buttergotchiConfig.bgColor = color;
        }
        return;
    }
}

/*********************************************************************
**  Function: setSoundConfig - 01/2026 - Refactored "ConfigMenu" (this function manteined for
* retrocompatibility)
**  Enable or disable sound
**********************************************************************/
void setSoundConfig() {
    options = {
        {"Sound off", [=]() { buttergotchiConfig.setSoundEnabled(0); }, buttergotchiConfig.soundEnabled == 0},
        {"Sound on",  [=]() { buttergotchiConfig.setSoundEnabled(1); }, buttergotchiConfig.soundEnabled == 1},
    };
    loopOptions(options, buttergotchiConfig.soundEnabled);
}

/*********************************************************************
**  Function: setSoundVolume
**  Set sound volume
**********************************************************************/
void setSoundVolume() {
    options = {
        {"10%",  [=]() { buttergotchiConfig.setSoundVolume(10); },  buttergotchiConfig.soundVolume == 10 },
        {"20%",  [=]() { buttergotchiConfig.setSoundVolume(20); },  buttergotchiConfig.soundVolume == 20 },
        {"30%",  [=]() { buttergotchiConfig.setSoundVolume(30); },  buttergotchiConfig.soundVolume == 30 },
        {"40%",  [=]() { buttergotchiConfig.setSoundVolume(40); },  buttergotchiConfig.soundVolume == 40 },
        {"50%",  [=]() { buttergotchiConfig.setSoundVolume(50); },  buttergotchiConfig.soundVolume == 50 },
        {"60%",  [=]() { buttergotchiConfig.setSoundVolume(60); },  buttergotchiConfig.soundVolume == 60 },
        {"70%",  [=]() { buttergotchiConfig.setSoundVolume(70); },  buttergotchiConfig.soundVolume == 70 },
        {"80%",  [=]() { buttergotchiConfig.setSoundVolume(80); },  buttergotchiConfig.soundVolume == 80 },
        {"90%",  [=]() { buttergotchiConfig.setSoundVolume(90); },  buttergotchiConfig.soundVolume == 90 },
        {"100%", [=]() { buttergotchiConfig.setSoundVolume(100); }, buttergotchiConfig.soundVolume == 100},
    };
    loopOptions(options, buttergotchiConfig.soundVolume);
}

#ifdef HAS_RGB_LED
/*********************************************************************
**  Function: setLedBlinkConfig - 01/2026 - Refactored "ConfigMenu" (this function manteined for
* retrocompatibility)
**  Enable or disable led blink
**********************************************************************/
void setLedBlinkConfig() {
    options = {
        {"Led Blink off", [=]() { buttergotchiConfig.setLedBlinkEnabled(0); }, buttergotchiConfig.ledBlinkEnabled == 0},
        {"Led Blink on",  [=]() { buttergotchiConfig.setLedBlinkEnabled(1); }, buttergotchiConfig.ledBlinkEnabled == 1},
    };
    loopOptions(options, buttergotchiConfig.ledBlinkEnabled);
}
#endif

/*********************************************************************
**  Function: setWifiStartupConfig
**  Enable or disable wifi connection at startup
**********************************************************************/
void setWifiStartupConfig() {
    options = {
        {"Disable", [=]() { buttergotchiConfig.setWifiAtStartup(0); }, buttergotchiConfig.wifiAtStartup == 0},
        {"Enable",  [=]() { buttergotchiConfig.setWifiAtStartup(1); }, buttergotchiConfig.wifiAtStartup == 1},
    };
    loopOptions(options, buttergotchiConfig.wifiAtStartup);
}

/*********************************************************************
**  Function: addEvilWifiMenu
**  Handles Menu to add evil wifi names into config list
**********************************************************************/
void addEvilWifiMenu() {
    String apName = keyboard("", 30, "Evil Portal SSID");
    if (apName != "\x1B") buttergotchiConfig.addEvilWifiName(apName);
}

/*********************************************************************
**  Function: removeEvilWifiMenu
**  Handles Menu to remove evil wifi names from config list
**********************************************************************/
void removeEvilWifiMenu() {
    options = {};

    for (const auto &wifi_name : buttergotchiConfig.evilWifiNames) {
        options.push_back({wifi_name.c_str(), [wifi_name]() { buttergotchiConfig.removeEvilWifiName(wifi_name); }});
    }

    options.push_back({"Cancel", [=]() { backToMenu(); }});

    loopOptions(options);
}

/*********************************************************************
**  Function: setEvilEndpointCreds
**  Handles menu for changing the endpoint to access captured creds
**********************************************************************/
void setEvilEndpointCreds() {
    String userInput = keyboard(buttergotchiConfig.evilPortalEndpoints.getCredsEndpoint, 30, "Evil creds endpoint");
    if (userInput != "\x1B") buttergotchiConfig.setEvilEndpointCreds(userInput);
}

/*********************************************************************
**  Function: setEvilEndpointSsid
**  Handles menu for changing the endpoint to change evilSsid
**********************************************************************/
void setEvilEndpointSsid() {
    String userInput = keyboard(buttergotchiConfig.evilPortalEndpoints.setSsidEndpoint, 30, "Evil creds endpoint");
    if (userInput != "\x1B") buttergotchiConfig.setEvilEndpointSsid(userInput);
}

/*********************************************************************
**  Function: setEvilAllowGetCredentials
**  Handles menu for toggling access to the credential list endpoint
**********************************************************************/

void setEvilAllowGetCreds() {
    options = {
        {"Disallow",
         [=]() { buttergotchiConfig.setEvilAllowGetCreds(false); },
         buttergotchiConfig.evilPortalEndpoints.allowGetCreds == false},
        {"Allow",
         [=]() { buttergotchiConfig.setEvilAllowGetCreds(true); },
         buttergotchiConfig.evilPortalEndpoints.allowGetCreds == true },
    };
    loopOptions(options, buttergotchiConfig.evilPortalEndpoints.allowGetCreds);
}

/*********************************************************************
**  Function: setEvilAllowGetCredentials
**  Handles menu for toggling access to the change SSID endpoint
**********************************************************************/

void setEvilAllowSetSsid() {
    options = {
        {"Disallow",
         [=]() { buttergotchiConfig.setEvilAllowSetSsid(false); },
         buttergotchiConfig.evilPortalEndpoints.allowSetSsid == false},
        {"Allow",
         [=]() { buttergotchiConfig.setEvilAllowSetSsid(true); },
         buttergotchiConfig.evilPortalEndpoints.allowSetSsid == true },
    };
    loopOptions(options, buttergotchiConfig.evilPortalEndpoints.allowSetSsid);
}

/*********************************************************************
**  Function: setEvilAllowEndpointDisplay
**  Handles menu for toggling the display of the Evil Portal endpoints
**********************************************************************/

void setEvilAllowEndpointDisplay() {
    options = {
        {"Disallow",
         [=]() { buttergotchiConfig.setEvilAllowEndpointDisplay(false); },
         buttergotchiConfig.evilPortalEndpoints.showEndpoints == false},
        {"Allow",
         [=]() { buttergotchiConfig.setEvilAllowEndpointDisplay(true); },
         buttergotchiConfig.evilPortalEndpoints.showEndpoints == true },
    };
    loopOptions(options, buttergotchiConfig.evilPortalEndpoints.showEndpoints);
}

/*********************************************************************
** Function: setEvilPasswordMode
** Handles menu for setting the evil portal password mode
***********************************************************************/
void setEvilPasswordMode() {
    options = {
        {"Save 'password'",
         [=]() { buttergotchiConfig.setEvilPasswordMode(FULL_PASSWORD); },
         buttergotchiConfig.evilPortalPasswordMode == FULL_PASSWORD  },
        {"Save 'p******d'",
         [=]() { buttergotchiConfig.setEvilPasswordMode(FIRST_LAST_CHAR); },
         buttergotchiConfig.evilPortalPasswordMode == FIRST_LAST_CHAR},
        {"Save '*hidden*'",
         [=]() { buttergotchiConfig.setEvilPasswordMode(HIDE_PASSWORD); },
         buttergotchiConfig.evilPortalPasswordMode == HIDE_PASSWORD  },
        {"Save length",
         [=]() { buttergotchiConfig.setEvilPasswordMode(SAVE_LENGTH); },
         buttergotchiConfig.evilPortalPasswordMode == SAVE_LENGTH    },
    };
    loopOptions(options, buttergotchiConfig.evilPortalPasswordMode);
}

/*********************************************************************
** Function: setEvilGatewayIp
** Handles menu for setting the Evil Portal gateway IP
***********************************************************************/
void setEvilGatewayIp() {
    options = {
        {"172.0.0.1",
         [=]() { buttergotchiConfig.setEvilGatewayIp("172.0.0.1"); },
         buttergotchiConfig.evilPortalGatewayIp == "172.0.0.1"},
        {"192.168.4.1",
         [=]() { buttergotchiConfig.setEvilGatewayIp("192.168.4.1"); },
         buttergotchiConfig.evilPortalGatewayIp == "192.168.4.1"},
        {"Custom", [=]() {
             String ip = num_keyboard("", 15, "Gateway Addr");
             buttergotchiConfig.setEvilGatewayIp(ip);
         }},
    };
    loopOptions(options, buttergotchiConfig.evilPortalGatewayIp == "192.168.4.1" ? 1 : 0);
}

/*********************************************************************
**  Function: setRFModuleMenu
**  Handles Menu to set the RF module in use
**********************************************************************/
void setRFModuleMenu() {
    int result = 0;
    int idx = 0;
    uint8_t pins_setup = 0;
    if (buttergotchiConfigPins.rfModule == M5_RF_MODULE) idx = 0;
    else if (buttergotchiConfigPins.rfModule == CC1101_SPI_MODULE) {
        idx = 1;
#if defined(ARDUINO_M5STICK_C_PLUS) || defined(ARDUINO_M5STICK_C_PLUS2)
        if (buttergotchiConfigPins.CC1101_bus.mosi == GPIO_NUM_26) idx = 2;
#endif
    }

    options = {
        {"M5 RF433T/R",         [&]() { result = M5_RF_MODULE; }   },
#if defined(ARDUINO_M5STICK_C_PLUS) || defined(ARDUINO_M5STICK_C_PLUS2)
        {"CC1101 (legacy)",     [&pins_setup]() { pins_setup = 1; }},
        {"CC1101 (Shared SPI)", [&pins_setup]() { pins_setup = 2; }},
#else
        {"CC1101", [&]() { result = CC1101_SPI_MODULE; }},
#endif
        /* WIP:
         * #ifdef USE_CC1101_VIA_PCA9554
         * {"CC1101+PCA9554",  [&]() { result = 2; }},
         * #endif
         */
    };
    loopOptions(options, idx);
    if (result == CC1101_SPI_MODULE || pins_setup > 0) {
        // This setting is meant to StickCPlus and StickCPlus2 to setup the ports from RF Menu
        if (pins_setup == 1) {
            result = CC1101_SPI_MODULE;
            buttergotchiConfigPins.setCC1101Pins(
                {(gpio_num_t)CC1101_SCK_PIN,
                 (gpio_num_t)CC1101_MISO_PIN,
                 (gpio_num_t)CC1101_MOSI_PIN,
                 (gpio_num_t)CC1101_SS_PIN,
                 (gpio_num_t)CC1101_GDO0_PIN,
                 GPIO_NUM_NC}
            );
            buttergotchiConfigPins.setNrf24Pins(
                {(gpio_num_t)CC1101_SCK_PIN,
                 (gpio_num_t)CC1101_MISO_PIN,
                 (gpio_num_t)CC1101_MOSI_PIN,
                 (gpio_num_t)CC1101_SS_PIN,
                 (gpio_num_t)CC1101_GDO0_PIN,
                 GPIO_NUM_NC}
            );
        } else if (pins_setup == 2) {
#if CONFIG_SOC_GPIO_OUT_RANGE_MAX > 30
            result = CC1101_SPI_MODULE;
            buttergotchiConfigPins.setCC1101Pins(
                {(gpio_num_t)SDCARD_SCK,
                 (gpio_num_t)SDCARD_MISO,
                 (gpio_num_t)SDCARD_MOSI,
                 GPIO_NUM_33,
                 GPIO_NUM_32,
                 GPIO_NUM_NC}
            );
            buttergotchiConfigPins.setNrf24Pins(
                {(gpio_num_t)SDCARD_SCK,
                 (gpio_num_t)SDCARD_MISO,
                 (gpio_num_t)SDCARD_MOSI,
                 GPIO_NUM_33,
                 GPIO_NUM_32,
                 GPIO_NUM_NC}
            );
#endif
        }
        displayError("CC1101 not found", true);
        while (!check(AnyKeyPress)) vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    // fallback to "M5 RF433T/R" on errors
    buttergotchiConfigPins.setRfModule(M5_RF_MODULE);
}

/*********************************************************************
**  Function: setRFFreqMenu
**  Handles Menu to set the default frequency for the RF module
**********************************************************************/
void setRFFreqMenu() {
    float result = 433.92;
    String freq_str = num_keyboard(String(buttergotchiConfigPins.rfFreq), 10, "Default frequency:");
    if (freq_str == "\x1B") return;
    if (freq_str.length() > 1) {
        result = freq_str.toFloat();          // returns 0 if not valid
        if (result >= 280 && result <= 928) { // TODO: check valid freq according to current module?
            buttergotchiConfigPins.setRfFreq(result);
            return;
        }
    }
    // else
    displayError("Invalid frequency");
    buttergotchiConfigPins.setRfFreq(433.92); // reset to default
    delay(1000);
}

/*********************************************************************
**  Function: setRFIDModuleMenu
**  Handles Menu to set the RFID module in use
**********************************************************************/
void setRFIDModuleMenu() {
    options = {
        {"M5 RFID2",
         [=]() { buttergotchiConfigPins.setRfidModule(M5_RFID2_MODULE); },
         buttergotchiConfigPins.rfidModule == M5_RFID2_MODULE     },
#ifdef M5STICK
        {"PN532 I2C G33",
         [=]() { buttergotchiConfigPins.setRfidModule(PN532_I2C_MODULE); },
         buttergotchiConfigPins.rfidModule == PN532_I2C_MODULE    },
        {"PN532 I2C G36",
         [=]() { buttergotchiConfigPins.setRfidModule(PN532_I2C_SPI_MODULE); },
         buttergotchiConfigPins.rfidModule == PN532_I2C_SPI_MODULE},
#else
        {"PN532 on I2C",
         [=]() { buttergotchiConfigPins.setRfidModule(PN532_I2C_MODULE); },
         buttergotchiConfigPins.rfidModule == PN532_I2C_MODULE},
#endif
        {"PN532 on SPI",
         [=]() { buttergotchiConfigPins.setRfidModule(PN532_SPI_MODULE); },
         buttergotchiConfigPins.rfidModule == PN532_SPI_MODULE    },
        {"RC522 on SPI",
         [=]() { buttergotchiConfigPins.setRfidModule(RC522_SPI_MODULE); },
         buttergotchiConfigPins.rfidModule == RC522_SPI_MODULE    },
    };
    loopOptions(options, buttergotchiConfigPins.rfidModule);
}

/*********************************************************************
**  Function: addMifareKeyMenu
**  Handles Menu to add MIFARE keys into config list
**********************************************************************/
void addMifareKeyMenu() {
    String key = keyboard("", 12, "MIFARE key");
    if (key != "\x1B") buttergotchiConfig.addMifareKey(key);
}

/*********************************************************************
**  Function: setClock
**  Handles Menu to set timezone to NTP
**********************************************************************/
const char *ntpServer = "pool.ntp.org";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, 0, 0);

void setClock() {
#if defined(HAS_RTC)
    RTC_TimeTypeDef TimeStruct;
#if defined(HAS_RTC_BM8563)
    _rtc.GetBm8563Time();
#endif
#if defined(HAS_RTC_PCF85063A)
    _rtc.GetPcf85063Time();
#endif
#endif

    options = {
        {"Via NTP Set Timezone",                                                 [&]() { buttergotchiConfig.setAutomaticTimeUpdateViaNTP(true); } },
        {"Set Time Manually",                                                    [&]() { buttergotchiConfig.setAutomaticTimeUpdateViaNTP(false); }},
        {("Daylight Savings " + String(buttergotchiConfig.dst ? "On" : "Off")).c_str(),
         [&]() {
             buttergotchiConfig.setDST(!buttergotchiConfig.dst);
             updateClockTimezone();
             returnToMenu = true;
         }                                                                                                                                 },
        {(buttergotchiConfig.clock24hr ? "24-Hour Format" : "12-Hour Format"),          [&]() {
             buttergotchiConfig.setClock24Hr(!buttergotchiConfig.clock24hr);
             returnToMenu = true;
         }                                                          }
    };

    addOptionToMainMenu();
    loopOptions(options);

    if (returnToMenu) return;

    if (buttergotchiConfig.automaticTimeUpdateViaNTP) {
        if (!wifiConnected) wifiConnectMenu();

        options.clear();

#ifndef LITE_VERSION

        struct TimezoneMapping {
            const char *name;
            float offset;
        };

        constexpr TimezoneMapping timezoneMappings[] = {
            {"UTC-12 (Baker Island, Howland Island)",     -12  },
            {"UTC-11 (Niue, Pago Pago)",                  -11  },
            {"UTC-10 (Honolulu, Papeete)",                -10  },
            {"UTC-9 (Anchorage, Gambell)",                -9   },
            {"UTC-9.5 (Marquesas Islands)",               -9.5 },
            {"UTC-8 (Los Angeles, Vancouver, Tijuana)",   -8   },
            {"UTC-7 (Denver, Phoenix, Edmonton)",         -7   },
            {"UTC-6 (Mexico City, Chicago, Tegucigalpa)", -6   },
            {"UTC-5 (New York, Toronto, Lima)",           -5   },
            {"UTC-4 (Caracas, Santiago, La Paz)",         -4   },
            {"UTC-3 (Brasilia, Sao Paulo, Montevideo)",   -3   },
            {"UTC-2 (South Georgia, Mid-Atlantic)",       -2   },
            {"UTC-1 (Azores, Cape Verde)",                -1   },
            {"UTC+0 (London, Lisbon, Casablanca)",        0    },
            {"UTC+0.5 (Tehran)",                          0.5  },
            {"UTC+1 (Berlin, Paris, Rome)",               1    },
            {"UTC+2 (Cairo, Athens, Johannesburg)",       2    },
            {"UTC+3 (Moscow, Riyadh, Nairobi)",           3    },
            {"UTC+3.5 (Tehran)",                          3.5  },
            {"UTC+4 (Dubai, Baku, Muscat)",               4    },
            {"UTC+4.5 (Kabul)",                           4.5  },
            {"UTC+5 (Islamabad, Karachi, Tashkent)",      5    },
            {"UTC+5.5 (New Delhi, Mumbai, Colombo)",      5.5  },
            {"UTC+5.75 (Kathmandu)",                      5.75 },
            {"UTC+6 (Dhaka, Almaty, Omsk)",               6    },
            {"UTC+6.5 (Yangon, Cocos Islands)",           6.5  },
            {"UTC+7 (Bangkok, Jakarta, Hanoi)",           7    },
            {"UTC+8 (Beijing, Singapore, Perth)",         8    },
            {"UTC+8.75 (Eucla)",                          8.75 },
            {"UTC+9 (Tokyo, Seoul, Pyongyang)",           9    },
            {"UTC+9.5 (Adelaide, Darwin)",                9.5  },
            {"UTC+10 (Sydney, Melbourne, Vladivostok)",   10   },
            {"UTC+10.5 (Lord Howe Island)",               10.5 },
            {"UTC+11 (Solomon Islands, Nouméa)",          11   },
            {"UTC+12 (Auckland, Fiji, Kamchatka)",        12   },
            {"UTC+12.75 (Chatham Islands)",               12.75},
            {"UTC+13 (Tonga, Phoenix Islands)",           13   },
            {"UTC+14 (Kiritimati)",                       14   }
        };

        int idx = 0;
        int i = 0;
        for (const auto &mapping : timezoneMappings) {
            if (buttergotchiConfig.tmz == mapping.offset) { idx = i; }

            options.emplace_back(
                mapping.name, [=, &mapping]() { buttergotchiConfig.setTmz(mapping.offset); }, idx == i
            );
            ++i;
        }

#else
        constexpr float timezoneOffsets[] = {-12, -11, -10,  -9.5, -9,  -8,    -7, -6, -5,   -4,
                                             -3,  -2,  -1,   0,    0.5, 1,     2,  3,  3.5,  4,
                                             4.5, 5,   5.5,  5.75, 6,   6.5,   7,  8,  8.75, 9,
                                             9.5, 10,  10.5, 11,   12,  12.75, 13, 14};

        int idx = 0;
        int i = 0;
        for (const auto &offset : timezoneOffsets) {
            if (buttergotchiConfig.tmz == offset) idx = i;

            options.emplace_back(
                ("UTC" + String(offset >= 0 ? "+" : "") + String(offset)).c_str(),
                [=]() { buttergotchiConfig.setTmz(offset); },
                buttergotchiConfig.tmz == offset
            );
            ++i;
        }

#endif

        addOptionToMainMenu();

        loopOptions(options, idx);

        updateClockTimezone();

    } else {
        int hr, mn, am = 0; // Initialize am to default value
        options = {};
        for (int i = 0; i < 12; i++) {
            String tmp = String(i < 10 ? "0" : "") + String(i);
            options.push_back({tmp.c_str(), [&]() { delay(1); }});
        }

        hr = loopOptions(options, MENU_TYPE_SUBMENU, "Set Hour");
        options.clear();

        for (int i = 0; i < 60; i++) {
            String tmp = String(i < 10 ? "0" : "") + String(i);
            options.push_back({tmp.c_str(), [&]() { delay(1); }});
        }

        mn = loopOptions(options, MENU_TYPE_SUBMENU, "Set Minute");
        options.clear();

        options = {
            {"AM", [&]() { am = 0; } },
            {"PM", [&]() { am = 12; }},
        };

        loopOptions(options);

#if defined(HAS_RTC)
        TimeStruct.Hours = hr + am;
        TimeStruct.Minutes = mn;
        TimeStruct.Seconds = 0;
        _rtc.SetTime(&TimeStruct);
        _rtc.GetTime(&_time);
        _rtc.GetDate(&_date);

        struct tm timeinfo = {};
        timeinfo.tm_sec = _time.Seconds;
        timeinfo.tm_min = _time.Minutes;
        timeinfo.tm_hour = _time.Hours;
        timeinfo.tm_mday = _date.Date;
        timeinfo.tm_mon = _date.Month > 0 ? _date.Month - 1 : 0;
        timeinfo.tm_year = _date.Year >= 1900 ? _date.Year - 1900 : 0;
        time_t epoch = mktime(&timeinfo);
        struct timeval tv = {.tv_sec = epoch};
        settimeofday(&tv, nullptr);
#else
        rtc.setTime(0, mn, hr + am, 20, 06, CURRENT_YEAR); // send me a gift, @Pirata!
        struct tm t = rtc.getTimeStruct();
        time_t epoch = mktime(&t);
        struct timeval tv = {.tv_sec = epoch};
        settimeofday(&tv, nullptr);
#endif
        clock_set = true;
    }
}

void runClockLoop(bool showMenuHint) {
    int tmp = 0;
    unsigned long hintStartTime = millis();
    bool hintVisible = showMenuHint;

#if defined(HAS_RTC)
#if defined(HAS_RTC_BM8563)
    _rtc.GetBm8563Time();
#endif
#if defined(HAS_RTC_PCF85063A)
    _rtc.GetPcf85063Time();
#endif
    _rtc.GetTime(&_time);
#endif

    // Delay due to SelPress() detected on run
    tft.fillScreen(buttergotchiConfig.bgColor);
    delay(300);

    for (;;) {
        if (millis() - tmp > 1000) {
#if defined(HAS_RTC)
            updateTimeStr(_rtc.getTimeStruct());
#else
            updateTimeStr(rtc.getTimeStruct());
#endif
            Serial.print("Current time: ");
            Serial.println(timeStr);
            tft.setTextColor(buttergotchiConfig.priColor, buttergotchiConfig.bgColor);
            tft.drawRect(
                BORDER_PAD_X,
                BORDER_PAD_X,
                tftWidth - 2 * BORDER_PAD_X,
                tftHeight - 2 * BORDER_PAD_X,
                buttergotchiConfig.priColor
            );
            uint8_t f_size = 4;
            for (uint8_t i = 4; i > 0; i--) {
                if (i * LW * strlen(timeStr) < (tftWidth - BORDER_PAD_X * 2)) {
                    f_size = i;
                    break;
                }
            }
            tft.setTextSize(f_size);
            tft.drawCentreString(timeStr, tftWidth / 2, tftHeight / 2 - 13, 1);

            // "OK to show menu" hint management
            if (hintVisible && (millis() - hintStartTime < 5000)) {
                tft.setTextSize(1);
                tft.drawCentreString("OK to show menu", tftWidth / 2, tftHeight / 2 + 25, 1);
            } else if (hintVisible && (millis() - hintStartTime >= 5000)) {
                // Clear hint after 5 seconds
                tft.fillRect(
                    BORDER_PAD_X + 1,
                    tftHeight / 2 + 20,
                    tftWidth - 2 * BORDER_PAD_X - 2,
                    20,
                    buttergotchiConfig.bgColor
                );
                hintVisible = false;
            }
            tmp = millis();
        }

        // Checks to exit the loop
        if (check(SelPress)) {
            tft.fillScreen(buttergotchiConfig.bgColor);
            if (showMenuHint) {
                // Exits the loop to return to the caller (ClockMenu)
                break;
            } else {
                // Original behavior
                returnToMenu = true;
                break;
            }
        }

        if (check(EscPress)) {
            tft.fillScreen(buttergotchiConfig.bgColor);
            returnToMenu = true;
            break;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

/*********************************************************************
**  Function: gsetIrTxPin
**  get or set IR Tx Pin
**********************************************************************/
int gsetIrTxPin(bool set) {
    int result = buttergotchiConfigPins.irTx;

    if (result > 50) buttergotchiConfigPins.setIrTxPin(TXLED);
    if (set) {
        options.clear();
        std::vector<std::pair<const char *, int>> pins;
        pins = IR_TX_PINS;
        int idx = 100;
        int j = 0;
        for (auto pin : pins) {
            if (pin.second == buttergotchiConfigPins.irTx && idx == 100) idx = j;
            j++;
#ifdef ALLOW_ALL_GPIO_FOR_IR_RF
            int i = pin.second;
            if (i != TFT_CS && i != TFT_RST && i != TFT_SCLK && i != TFT_MOSI && i != TFT_BL &&
                i != TOUCH_CS && i != SDCARD_CS && i != SDCARD_MOSI && i != SDCARD_MISO)
#endif
                options.push_back(
                    {pin.first,
                     [=]() { buttergotchiConfigPins.setIrTxPin(pin.second); },
                     pin.second == buttergotchiConfigPins.irTx}
                );
        }

        loopOptions(options, idx);
        options.clear();

        Serial.println("Saved pin: " + String(buttergotchiConfigPins.irTx));
    }

    returnToMenu = true;
    return buttergotchiConfigPins.irTx;
}

void setIrTxRepeats() {
    uint8_t chRpts = 0; // Chosen Repeats

    options = {
        {"None",             [&]() { chRpts = 0; } },
        {"5  (+ 1 initial)", [&]() { chRpts = 5; } },
        {"10 (+ 1 initial)", [&]() { chRpts = 10; }},
        {"Custom",           [&]() {
             // up to 99 repeats
             String rpt =
                 num_keyboard(String(buttergotchiConfigPins.irTxRepeats), 2, "Nbr of Repeats (+ 1 initial)");
             chRpts = static_cast<uint8_t>(rpt.toInt());
         }                       },
    };
    addOptionToMainMenu();

    loopOptions(options);

    if (returnToMenu) return;

    buttergotchiConfigPins.setIrTxRepeats(chRpts);
}
/*********************************************************************
**  Function: gsetIrRxPin
**  get or set IR Rx Pin
**********************************************************************/
int gsetIrRxPin(bool set) {
    int result = buttergotchiConfigPins.irRx;

    if (result > 45) buttergotchiConfigPins.setIrRxPin(GROVE_SCL);
    if (set) {
        options.clear();
        std::vector<std::pair<const char *, int>> pins;
        pins = IR_RX_PINS;
        int idx = -1;
        int j = 0;
        for (auto pin : pins) {
            if (pin.second == buttergotchiConfigPins.irRx && idx < 0) idx = j;
            j++;
#ifdef ALLOW_ALL_GPIO_FOR_IR_RF
            int i = pin.second;
            if (i != TFT_CS && i != TFT_RST && i != TFT_SCLK && i != TFT_MOSI && i != TFT_BL &&
                i != TOUCH_CS && i != SDCARD_CS && i != SDCARD_MOSI && i != SDCARD_MISO)
#endif
                options.push_back(
                    {pin.first,
                     [=]() { buttergotchiConfigPins.setIrRxPin(pin.second); },
                     pin.second == buttergotchiConfigPins.irRx}
                );
        }

        loopOptions(options);
    }

    returnToMenu = true;
    return buttergotchiConfigPins.irRx;
}

/*********************************************************************
**  Function: gsetRfTxPin
**  get or set RF Tx Pin
**********************************************************************/
int gsetRfTxPin(bool set) {
    int result = buttergotchiConfigPins.rfTx;

    if (result > 45) buttergotchiConfigPins.setRfTxPin(GROVE_SDA);
    if (set) {
        options.clear();
        std::vector<std::pair<const char *, int>> pins;
        pins = RF_TX_PINS;
        int idx = -1;
        int j = 0;
        for (auto pin : pins) {
            if (pin.second == buttergotchiConfigPins.rfTx && idx < 0) idx = j;
            j++;
#ifdef ALLOW_ALL_GPIO_FOR_IR_RF
            int i = pin.second;
            if (i != TFT_CS && i != TFT_RST && i != TFT_SCLK && i != TFT_MOSI && i != TFT_BL &&
                i != TOUCH_CS && i != SDCARD_CS && i != SDCARD_MOSI && i != SDCARD_MISO)
#endif
                options.push_back(
                    {pin.first,
                     [=]() { buttergotchiConfigPins.setRfTxPin(pin.second); },
                     pin.second == buttergotchiConfigPins.rfTx}
                );
        }

        loopOptions(options);
        options.clear();
    }

    returnToMenu = true;
    return buttergotchiConfigPins.rfTx;
}

/*********************************************************************
**  Function: gsetRfRxPin
**  get or set FR Rx Pin
**********************************************************************/
int gsetRfRxPin(bool set) {
    int result = buttergotchiConfigPins.rfRx;

    if (result > 36) buttergotchiConfigPins.setRfRxPin(GROVE_SCL);
    if (set) {
        options.clear();
        std::vector<std::pair<const char *, int>> pins;
        pins = RF_RX_PINS;
        int idx = -1;
        int j = 0;
        for (auto pin : pins) {
            if (pin.second == buttergotchiConfigPins.rfRx && idx < 0) idx = j;
            j++;
#ifdef ALLOW_ALL_GPIO_FOR_IR_RF
            int i = pin.second;
            if (i != TFT_CS && i != TFT_RST && i != TFT_SCLK && i != TFT_MOSI && i != TFT_BL &&
                i != TOUCH_CS && i != SDCARD_CS && i != SDCARD_MOSI && i != SDCARD_MISO)
#endif
                options.push_back(
                    {pin.first,
                     [=]() { buttergotchiConfigPins.setRfRxPin(pin.second); },
                     pin.second == buttergotchiConfigPins.rfRx}
                );
        }

        loopOptions(options);
        options.clear();
    }

    returnToMenu = true;
    return buttergotchiConfigPins.rfRx;
}

/*********************************************************************
**  Function: setStartupApp
**  Handles Menu to set startup app
**********************************************************************/
void setStartupApp() {
    int idx = 0;
    if (buttergotchiConfig.startupApp == "") idx = 0;

    options = {
        {"None", [=]() { buttergotchiConfig.setStartupApp(""); }, buttergotchiConfig.startupApp == ""}
    };

    int index = 0;
    for (String appName : startupApp.getAppNames()) {
        index++;
        if (buttergotchiConfig.startupApp == appName) idx = index;

        options.push_back({appName.c_str(), [=]() {
                               buttergotchiConfig.setStartupApp(appName);
                           }});
    }

    loopOptions(options, idx);
    options.clear();
}

/*********************************************************************
**  Function: setGpsBaudrateMenu
**  Handles Menu to set the baudrate for the GPS module
**********************************************************************/
void setGpsBaudrateMenu() {
    options = {
        {"9600 bps",   [=]() { buttergotchiConfigPins.setGpsBaudrate(9600); },  buttergotchiConfigPins.gpsBaudrate == 9600 },
        {"19200 bps",  [=]() { buttergotchiConfigPins.setGpsBaudrate(19200); }, buttergotchiConfigPins.gpsBaudrate == 19200},
        {"38400 bps",  [=]() { buttergotchiConfigPins.setGpsBaudrate(38400); }, buttergotchiConfigPins.gpsBaudrate == 38400},
        {"57600 bps",  [=]() { buttergotchiConfigPins.setGpsBaudrate(57600); }, buttergotchiConfigPins.gpsBaudrate == 57600},
        {"115200 bps",
         [=]() { buttergotchiConfigPins.setGpsBaudrate(115200); },
         buttergotchiConfigPins.gpsBaudrate == 115200                                                               },
    };

    loopOptions(options, buttergotchiConfigPins.gpsBaudrate);
}

/*********************************************************************
**  Function: setWifiApSsidMenu
**  Handles Menu to set the WiFi AP SSID
**********************************************************************/
void setWifiApSsidMenu() {
    const bool isDefault = buttergotchiConfig.wifiAp.ssid == "HeavyButter";

    options = {
        {"Default (HeavyButter)",
         [=]() { buttergotchiConfig.setWifiApCreds("HeavyButter", buttergotchiConfig.wifiAp.pwd); },
         isDefault                                                                            },
        {"Custom",
         [=]() {
             String newSsid = keyboard(buttergotchiConfig.wifiAp.ssid, 32, "WiFi AP SSID:");
             if (newSsid != "\x1B") {
                 if (!newSsid.isEmpty()) buttergotchiConfig.setWifiApCreds(newSsid, buttergotchiConfig.wifiAp.pwd);
                 else displayError("SSID cannot be empty", true);
             }
         },                                                                         !isDefault},
    };
    addOptionToMainMenu();

    loopOptions(options, isDefault ? 0 : 1);
}

/*********************************************************************
**  Function: setWifiApPasswordMenu
**  Handles Menu to set the WiFi AP Password
**********************************************************************/
void setWifiApPasswordMenu() {
    const bool isDefault = buttergotchiConfig.wifiAp.pwd == "heavybutter";

    options = {
        {"Default (heavybutter)",
         [=]() { buttergotchiConfig.setWifiApCreds(buttergotchiConfig.wifiAp.ssid, "heavybutter"); },
         isDefault                                                                             },
        {"Custom",
         [=]() {
             String newPassword = keyboard(buttergotchiConfig.wifiAp.pwd, 32, "WiFi AP Password:", true);
             if (newPassword != "\x1B") {
                 if (!newPassword.isEmpty()) buttergotchiConfig.setWifiApCreds(buttergotchiConfig.wifiAp.ssid, newPassword);
                 else displayError("Password cannot be empty", true);
             }
         },                                                                          !isDefault},
    };
    addOptionToMainMenu();

    loopOptions(options, isDefault ? 0 : 1);
}

/*********************************************************************
**  Function: setWifiApCredsMenu
**  Handles Menu to configure WiFi AP Credentials
**********************************************************************/
void setWifiApCredsMenu() {
    options = {
        {"SSID",     setWifiApSsidMenu    },
        {"Password", setWifiApPasswordMenu},
    };
    addOptionToMainMenu();

    loopOptions(options);
}

/*********************************************************************
**  Function: setNetworkCredsMenu
**  Main Menu for setting Network credentials (BLE & WiFi)
**********************************************************************/
void setNetworkCredsMenu() {
    options = {
        {"WiFi AP Creds", setWifiApCredsMenu}
    };
    addOptionToMainMenu();

    loopOptions(options);
}

/*********************************************************************
**  Function: setBadUSBBLEMenu
**  Main Menu for setting Bad USB/BLE options
**********************************************************************/
void setBadUSBBLEMenu() {
    options = {
        {"Keyboard Layout", setBadUSBBLEKeyboardLayoutMenu},
        {"Key Delay",       setBadUSBBLEKeyDelayMenu      },
        {"Show Output",     setBadUSBBLEShowOutputMenu    },
    };
    addOptionToMainMenu();

    loopOptions(options);
}

/*********************************************************************
**  Function: setBadUSBBLEKeyboardLayoutMenu
**  Main Menu for setting Bad USB/BLE Keyboard Layout
**********************************************************************/
void setBadUSBBLEKeyboardLayoutMenu() {
    uint8_t opt = buttergotchiConfig.badUSBBLEKeyboardLayout;

    options.clear();
    options = {
        {"US International",      [&]() { opt = 0; } },
        {"Danish",                [&]() { opt = 1; } },
        {"English (UK)",          [&]() { opt = 2; } },
        {"French (AZERTY)",       [&]() { opt = 3; } },
        {"German",                [&]() { opt = 4; } },
        {"Hungarian",             [&]() { opt = 5; } },
        {"Italian",               [&]() { opt = 6; } },
        {"Polish",                [&]() { opt = 7; } },
        {"Portuguese (Brazil)",   [&]() { opt = 8; } },
        {"Portuguese (Portugal)", [&]() { opt = 9; } },
        {"Slovenian",             [&]() { opt = 10; }},
        {"Spanish",               [&]() { opt = 11; }},
        {"Swedish",               [&]() { opt = 12; }},
        {"Turkish",               [&]() { opt = 13; }},
    };
    addOptionToMainMenu();

    loopOptions(options, opt);

    if (opt != buttergotchiConfig.badUSBBLEKeyboardLayout) { buttergotchiConfig.setBadUSBBLEKeyboardLayout(opt); }
}

/*********************************************************************
**  Function: setBadUSBBLEKeyDelayMenu
**  Main Menu for setting Bad USB/BLE Keyboard Key Delay
**********************************************************************/
void setBadUSBBLEKeyDelayMenu() {
    String delayStr = num_keyboard(String(buttergotchiConfig.badUSBBLEKeyDelay), 3, "Key Delay (ms):");
    if (delayStr != "\x1B") {
        uint16_t delayVal = static_cast<uint16_t>(delayStr.toInt());
        if (delayVal <= 500) {
            buttergotchiConfig.setBadUSBBLEKeyDelay(delayVal);
        } else if (delayVal != 0) {
            displayError("Invalid key delay value (0 to 500)", true);
        }
    }
}

/*********************************************************************
**  Function: setBadUSBBLEShowOutputMenu
**  Main Menu for setting Bad USB/BLE Show Output
**********************************************************************/
void setBadUSBBLEShowOutputMenu() {
    options.clear();
    options = {
        {"Enable",  [&]() { buttergotchiConfig.setBadUSBBLEShowOutput(true); } },
        {"Disable", [&]() { buttergotchiConfig.setBadUSBBLEShowOutput(false); }},
    };
    addOptionToMainMenu();

    loopOptions(options, buttergotchiConfig.badUSBBLEShowOutput ? 0 : 1);
}

/*********************************************************************
**  Function: setMacAddressMenu - @IncursioHack
**  Handles Menu to configure WiFi MAC Address
**********************************************************************/
void setMacAddressMenu() {
    String currentMAC = buttergotchiConfig.wifiMAC;
    if (currentMAC == "") currentMAC = WiFi.macAddress();

    options.clear();
    options = {
        {"Default MAC (" + WiFi.macAddress() + ")",
         [&]() { buttergotchiConfig.setWifiMAC(""); },
         buttergotchiConfig.wifiMAC == ""},
        {"Set Custom MAC",
         [&]() {
             String newMAC = keyboard(buttergotchiConfig.wifiMAC, 17, "XX:YY:ZZ:AA:BB:CC");
             if (newMAC == "\x1B") return;
             if (newMAC.length() == 17) {
                 buttergotchiConfig.setWifiMAC(newMAC);
             } else {
                 displayError("Invalid MAC format");
             }
         }, buttergotchiConfig.wifiMAC != ""},
        {"Random MAC", [&]() {
             uint8_t randomMac[6];
             for (int i = 0; i < 6; i++) randomMac[i] = random(0x00, 0xFF);
             char buf[18];
             snprintf(
                 buf,
                 sizeof(buf),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 randomMac[0],
                 randomMac[1],
                 randomMac[2],
                 randomMac[3],
                 randomMac[4],
                 randomMac[5]
             );
             buttergotchiConfig.setWifiMAC(String(buf));
         }}
    };

    addOptionToMainMenu();
    loopOptions(options, MENU_TYPE_REGULAR, ("Current: " + currentMAC).c_str());
}

/*********************************************************************
**  Function: setSPIPins
**  Main Menu to manually set SPI Pins
**********************************************************************/
void setSPIPinsMenu(ButtergotchiConfigPins::SPIPins &value) {
    uint8_t opt = 0;
    bool changed = false;
    ButtergotchiConfigPins::SPIPins points = value;

RELOAD:
    options = {
        {String("SCK =" + String(points.sck)).c_str(), [&]() { opt = 1; }},
        {String("MISO=" + String(points.miso)).c_str(), [&]() { opt = 2; }},
        {String("MOSI=" + String(points.mosi)).c_str(), [&]() { opt = 3; }},
        {String("CS  =" + String(points.cs)).c_str(), [&]() { opt = 4; }},
        {String("CE/GDO0=" + String(points.io0)).c_str(), [&]() { opt = 5; }},
        {String("NC/GDO2=" + String(points.io2)).c_str(), [&]() { opt = 6; }},
        {"Save Config", [&]() { opt = 7; }, changed},
        {"Main Menu", [&]() { opt = 0; }},
    };

    loopOptions(options);
    if (opt == 0) return;
    else if (opt == 7) {
        if (changed) {
            value = points;
            buttergotchiConfigPins.setSpiPins(value);
        }
    } else {
        options = {};
        gpio_num_t sel = GPIO_NUM_NC;
        int index = 0;
        if (opt == 1) index = points.sck + 1;
        else if (opt == 2) index = points.miso + 1;
        else if (opt == 3) index = points.mosi + 1;
        else if (opt == 4) index = points.cs + 1;
        else if (opt == 5) index = points.io0 + 1;
        else if (opt == 6) index = points.io2 + 1;
        for (int8_t i = -1; i <= GPIO_NUM_MAX; i++) {
            String tmp = String(i);
            options.push_back({tmp.c_str(), [i, &sel]() { sel = (gpio_num_t)i; }});
        }
        loopOptions(options, index);
        options.clear();
        if (opt == 1) points.sck = sel;
        else if (opt == 2) points.miso = sel;
        else if (opt == 3) points.mosi = sel;
        else if (opt == 4) points.cs = sel;
        else if (opt == 5) points.io0 = sel;
        else if (opt == 6) points.io2 = sel;
        changed = true;
        goto RELOAD;
    }
}

/*********************************************************************
**  Function: setUARTPins
**  Main Menu to manually set SPI Pins
**********************************************************************/
void setUARTPinsMenu(ButtergotchiConfigPins::UARTPins &value) {
    uint8_t opt = 0;
    bool changed = false;
    ButtergotchiConfigPins::UARTPins points = value;

RELOAD:
    options = {
        {String("RX = " + String(points.rx)).c_str(), [&]() { opt = 1; }},
        {String("TX = " + String(points.tx)).c_str(), [&]() { opt = 2; }},
        {"Save Config", [&]() { opt = 7; }, changed},
        {"Main Menu", [&]() { opt = 0; }},
    };

    loopOptions(options);
    if (opt == 0) return;
    else if (opt == 7) {
        if (changed) {
            value = points;
            buttergotchiConfigPins.setUARTPins(value);
        }
    } else {
        options = {};
        gpio_num_t sel = GPIO_NUM_NC;
        int index = 0;
        if (opt == 1) index = points.rx + 1;
        else if (opt == 2) index = points.tx + 1;
        for (int8_t i = -1; i <= GPIO_NUM_MAX; i++) {
            String tmp = String(i);
            options.push_back({tmp.c_str(), [i, &sel]() { sel = (gpio_num_t)i; }});
        }
        loopOptions(options, index);
        options.clear();
        if (opt == 1) points.rx = sel;
        else if (opt == 2) points.tx = sel;
        changed = true;
        goto RELOAD;
    }
}

/*********************************************************************
**  Function: setI2CPins
**  Main Menu to manually set SPI Pins
**********************************************************************/
void setI2CPinsMenu(ButtergotchiConfigPins::I2CPins &value) {
    uint8_t opt = 0;
    bool changed = false;
    ButtergotchiConfigPins::I2CPins points = value;

RELOAD:
    options = {
        {String("SDA = " + String(points.sda)).c_str(), [&]() { opt = 1; }},
        {String("SCL = " + String(points.scl)).c_str(), [&]() { opt = 2; }},
        {"Save Config", [&]() { opt = 7; }, changed},
        {"Main Menu", [&]() { opt = 0; }},
    };

    loopOptions(options);
    if (opt == 0) return;
    else if (opt == 7) {
        if (changed) {
            value = points;
            buttergotchiConfigPins.setI2CPins(value);
        }
    } else {
        options = {};
        gpio_num_t sel = GPIO_NUM_NC;
        int index = 0;
        if (opt == 1) index = points.sda + 1;
        else if (opt == 2) index = points.scl + 1;
        for (int8_t i = -1; i <= GPIO_NUM_MAX; i++) {
            String tmp = String(i);
            options.push_back({tmp.c_str(), [i, &sel]() { sel = (gpio_num_t)i; }});
        }
        loopOptions(options, index);
        options.clear();
        if (opt == 1) points.sda = sel;
        else if (opt == 2) points.scl = sel;
        changed = true;
        goto RELOAD;
    }
}

/*********************************************************************
**  Function: setTheme
**  Menu to change Theme
**********************************************************************/
void setTheme() {
    FS *fs = &LittleFS;
    options = {
        {"Little FS", [&]() { fs = &LittleFS; }},
        {"Default",
         [&]() {
             buttergotchiConfig.removeTheme();
             buttergotchiConfig.themePath = "";
             buttergotchiConfig.theme.fs = 0;
             buttergotchiConfig.secColor = DEFAULT_SECCOLOR;
             buttergotchiConfig.bgColor = TFT_BLACK;
             buttergotchiConfig.setUiColor(DEFAULT_PRICOLOR);
#ifdef HAS_RGB_LED
             buttergotchiConfig.ledBright = 50;
             buttergotchiConfig.ledColor = 0x960064;
             buttergotchiConfig.ledEffect = 0;
             buttergotchiConfig.ledEffectSpeed = 5;
             buttergotchiConfig.ledEffectDirection = 1;
             ledSetup();
#endif
             buttergotchiConfig.saveFile();
             fs = nullptr;
         }                                     },
        {"Main Menu", [&]() { fs = nullptr; }  }
    };
    if (setupSdCard()) {
        options.insert(options.begin(), {"SD Card", [&]() { fs = &SD; }});
    }
    loopOptions(options);
    if (fs == nullptr) return;

    String filepath = loopSD(*fs, true, "JSON");
    if (buttergotchiConfig.openThemeFile(fs, filepath, true)) {
        buttergotchiConfig.themePath = filepath;
        if (fs == &LittleFS) buttergotchiConfig.theme.fs = 1;
        else if (fs == &SD) buttergotchiConfig.theme.fs = 2;
        else buttergotchiConfig.theme.fs = 0;

        buttergotchiConfig.saveFile();
    }
}
bool appStoreInstalled() {
    FS *fs;
    if (!getFsStorage(fs)) {
        log_i("Fail getting filesystem");
        return false;
    }

    return fs->exists("/ButtergotchiJS/Tools/App Store.js");
}

#include <HTTPClient.h>
void installAppStoreJS() {

    if (WiFi.status() != WL_CONNECTED) { wifiConnectMenu(WIFI_STA); }
    if (WiFi.status() != WL_CONNECTED) {
        displayWarning("WiFi not connected", true);
        return;
    }

    FS *fs;
    if (!getFsStorage(fs)) {
        log_i("Fail getting filesystem");
        return;
    }

    if (!fs->exists("/ButtergotchiJS")) {
        if (!fs->mkdir("/ButtergotchiJS")) {
            displayWarning("Failed to create /ButtergotchiJS directory", true);
            return;
        }
    }

    if (!fs->exists("/ButtergotchiJS/Tools")) {
        if (!fs->mkdir("/ButtergotchiJS/Tools")) {
            displayWarning("Failed to create /ButtergotchiJS/Tools directory", true);
            return;
        }
    }

    HTTPClient http;
    http.begin(APPSTORE_SERVER_URL "/service/appstore/");
    int httpCode = http.GET();
    if (httpCode != 200) {
        http.end();
        displayWarning("Failed to download App Store", true);
        return;
    }

    String content = http.getString();
    http.end();

    HTTPClient hashHttp;
    hashHttp.begin(APPSTORE_SERVER_URL "/service/appstore/appstore.sha256");
    int hashCode = hashHttp.GET();
    String expectedHash = "";
    if (hashCode == 200) {
        expectedHash = hashHttp.getString();
        expectedHash.trim();
    }
    hashHttp.end();

    if (expectedHash.length() > 0) {
        if (!verify_sha256((const uint8_t *)content.c_str(), content.length(), expectedHash.c_str())) {
            displayWarning("App Store integrity check failed", true);
            Serial.println("[WARN] App Store SHA-256 mismatch.");
            return;
        }
    } else {
        Serial.println("[INFO] App Store SHA-256 not available — downloading without verification.");
    }

    File file = fs->open("/ButtergotchiJS/Tools/App Store.js", FILE_WRITE);
    if (!file) {
        displayWarning("Failed to save App Store", true);
        return;
    }
    file.print(content);
    file.close();


    displaySuccess("App Store installed", true);
    displaySuccess("Goto JS Interpreter -> Tools -> App Store", true);
}
