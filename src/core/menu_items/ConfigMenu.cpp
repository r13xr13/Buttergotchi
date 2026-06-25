#include "ConfigMenu.h"
#include "../mykeyboard.h"
#include "core/display.h"
#include "core/main_menu.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h"
#ifdef HAS_RGB_LED
#include "core/led_control.h"
#endif

/*********************************************************************
**  Function: optionsMenu
**  Main Config menu entry point
**********************************************************************/
void ConfigMenu::optionsMenu() {
    returnToMenu = false;
    while (true) {
        // Check if we need to exit to Main Menu (e.g., DevMode disabled)
        if (returnToMenu) {
            returnToMenu = false; // Reset flag
            return;
        }

        std::vector<Option> localOptions = {
            {"Display & UI",    [this]() { displayUIMenu(); }},
#ifdef HAS_RGB_LED
            {"LED Config",      [this]() { ledMenu(); }      },
#endif
            {"System Config",   [this]() { systemMenu(); }   },
            {"Power",           [this]() { powerMenu(); }    },
        };

        if (buttergotchiConfig.devMode) {
            localOptions.push_back({"Dev Mode", [this]() { devMenu(); }});
        }

        localOptions.push_back({"About", showDeviceInfo});
        localOptions.push_back({"Main Menu", []() {}});

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Config");

        // Exit to Main Menu only if user pressed Back
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Otherwise rebuild Config menu after submenu returns
    }
}

/*********************************************************************
**  Function: displayUIMenu
**  Display & UI configuration submenu with auto-rebuild
**********************************************************************/
void ConfigMenu::displayUIMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Brightness",  [this]() { setBrightnessMenu(); }               },
            {"Dim Time",    [this]() { setDimmerTimeMenu(); }               },
            {"Orientation", [this]() { lambdaHelper(gsetRotation, true)(); }},
            {"UI Color",    [this]() { setUIColor(); }                      },
            {"UI Theme",    [this]() { setTheme(); }                        },
            {"Back",        []() {}                                         },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Display & UI");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Otherwise loop continues and menu rebuilds
    }
}

/*********************************************************************
**  Function: ledMenu
**  LED configuration submenu with auto-rebuild for toggles
**********************************************************************/
#ifdef HAS_RGB_LED
void ConfigMenu::ledMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"LED Color",
             [this]() {
                 beginLed();
                 setLedColorConfig();
             }                                                                            },
            {"LED Effect",
             [this]() {
                 beginLed();
                 setLedEffectConfig();
             }                                                                            },
            {"LED Brightness",
             [this]() {
                 beginLed();
                 setLedBrightnessConfig();
             }                                                                            },
            {String("LED Blink: ") + (buttergotchiConfig.ledBlinkEnabled ? "ON" : "OFF"),
             [this]() {
                 // Toggle LED blink setting
                 buttergotchiConfig.ledBlinkEnabled = !buttergotchiConfig.ledBlinkEnabled;
                 buttergotchiConfig.saveFile();
             }                                                                            },
            {"Back",                                                               []() {}},
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "LED Config");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds to update toggle label
    }
}
#endif
/*********************************************************************
**  Function: systemMenu
**  System configuration submenu with auto-rebuild for toggles
**********************************************************************/
void ConfigMenu::systemMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {String("InstaBoot: ") + (buttergotchiConfig.instantBoot ? "ON" : "OFF"),
             [this]() {
                 // Toggle InstaBoot setting
                 buttergotchiConfig.instantBoot = !buttergotchiConfig.instantBoot;
                 buttergotchiConfig.saveFile();
             }                                                                                                           },
            {String("WiFi Startup: ") + (buttergotchiConfig.wifiAtStartup ? "ON" : "OFF"),
             [this]() {
                 // Toggle WiFi at startup setting
                 buttergotchiConfig.wifiAtStartup = !buttergotchiConfig.wifiAtStartup;
                 buttergotchiConfig.saveFile();
             }                                                                                                           },
            {"Startup App",                                                         [this]() { setStartupApp(); }        },
            {"Hide/Show Apps",                                                      [this]() { mainMenu.hideAppsMenu(); }},
            {"Clock",                                                               [this]() { setClock(); }             },
            {String("Keyboard Language: ") + buttergotchiConfig.keyboardLang,              [this]() { setKeyboardLanguage(); }  },
            {"Advanced",                                                            [this]() { advancedMenu(); }         },
            {"Back",                                                                []() {}                              },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "System Config");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds to update toggle labels
    }
}

/*********************************************************************
**  Function: advancedMenu
**  Advanced settings submenu (nested under System Config)
**********************************************************************/
void ConfigMenu::advancedMenu() {
    while (true) {
        std::vector<Option> localOptions = {

            {"Network Creds",  [this]() { setNetworkCredsMenu(); }},
            {"Factory Reset",
                                      []() {
                 // Confirmation dialog for destructive action
                 drawMainBorder(true);
                 int8_t choice = displayMessage(
                     "Are you sure you want\nto Factory Reset?\nAll data will be lost!",
                     "No",
                     nullptr,
                     "Yes",
                     TFT_RED
                 );

                 if (choice == 1) {
                     // User confirmed - perform factory reset
                     buttergotchiConfigPins.factoryReset();
                     buttergotchiConfig.factoryReset(); // Restarts ESP
                 }
                 // If cancelled, loop continues and menu rebuilds
             }                                                                             },
            {"Back",           []() {}                            },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Advanced");

        // Exit to System Config menu
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}
/*********************************************************************
**  Function: powerMenu
**  Power management submenu with auto-rebuild
**********************************************************************/
void ConfigMenu::powerMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Deep Sleep", goToDeepSleep          },
            {"Sleep",      setSleepMode           },
            {"Restart",    []() { ESP.restart(); }},
            {"Power Off",
             []() {
                 // Confirmation dialog for power off
                 drawMainBorder(true);
                 int8_t choice = displayMessage("Power Off Device?", "No", nullptr, "Yes", TFT_RED);

                 if (choice == 1) { powerOff(); }
             }                                    },
            {"Back",       []() {}                },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Power Menu");

        // Exit to Config menu
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}

/*********************************************************************
**  Function: devMenu
**  Developer mode menu for additional dev-only settings
**********************************************************************/
void ConfigMenu::devMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Disable DevMode", [this]() { buttergotchiConfig.setDevMode(false); }             },
            {"Back",            []() {}                                                 },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Dev Mode");

        // Check if "Disable DevMode" was pressed (second-to-last option)
        if (selected == localOptions.size() - 2) {
            returnToMenu = true; // Signal to exit all Config menus
            return;
        }

        // Exit to Config menu on Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}

/*********************************************************************
**  Function: drawIcon
**  Draw config gear icon
**********************************************************************/
void ConfigMenu::drawIcon(float scale) {
    clearIconArea();
    int radius = scale * 9;

    // Draw 6 gear teeth segments
    for (int i = 0; i < 6; i++) {
        tft.drawArc(
            iconCenterX,
            iconCenterY,
            3.5 * radius,
            2 * radius,
            15 + 60 * i,
            45 + 60 * i,
            buttergotchiConfig.priColor,
            buttergotchiConfig.bgColor,
            true
        );
    }

    // Draw inner circle
    tft.drawArc(
        iconCenterX,
        iconCenterY,
        2.5 * radius,
        radius,
        0,
        360,
        buttergotchiConfig.priColor,
        buttergotchiConfig.bgColor,
        false
    );
}
