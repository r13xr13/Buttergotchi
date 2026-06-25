#include "core/main_menu.h"
#include <globals.h>

#include "core/powerSave.h"
#include "core/serial_commands/cli.h"
#include "core/utils.h"
#include "current_year.h"
#include "esp32-hal-psram.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include <functional>
#include <string>
#include <vector>
#include "core/usb/usb_composite.h"
io_expander ioExpander;
ButtergotchiConfig buttergotchiConfig;
ButtergotchiConfigPins buttergotchiConfigPins;

SerialCli serialCli;
USBSerial USBserial;
SerialDevice *serialDevice = &USBserial;

StartupApp startupApp;
String startupAppJSInterpreterFile = "";

MainMenu mainMenu;
SPIClass sdcardSPI;
#ifdef USE_HSPI_PORT
#ifndef VSPI
#define VSPI FSPI
#endif
SPIClass CC_NRF_SPI(VSPI);
#else
SPIClass CC_NRF_SPI(HSPI);
#endif

// Shared SPI bus mutex for CC_NRF_SPI (coordinates access between NRF24, CC1101, LoRa, W5500)
SemaphoreHandle_t cc_nrf_spi_mutex = NULL;

// Navigation Variables
volatile bool NextPress = false;
volatile bool PrevPress = false;
volatile bool UpPress = false;
volatile bool DownPress = false;
volatile bool SelPress = false;
volatile bool EscPress = false;
volatile bool AnyKeyPress = false;
volatile bool NextPagePress = false;
volatile bool PrevPagePress = false;
volatile bool LongPress = false;
volatile bool SerialCmdPress = false;
volatile int forceMenuOption = -1;
volatile uint8_t menuOptionType = 0;
String menuOptionLabel = "";
#ifdef HAS_ENCODER_LED
volatile int EncoderLedChange = 0;
#endif

TouchPoint touchPoint;

keyStroke KeyStroke;

TaskHandle_t xHandle;
void __attribute__((weak)) taskInputHandler(void *parameter) {
    auto timer = millis();
    while (true) {
        checkPowerSaveTime();
        // Sometimes this task run 2 or more times before looptask,
        // and navigation gets stuck, the idea here is run the input detection
        // if AnyKeyPress is false, or rerun if it was not renewed within 75ms (arbitrary)
        // because AnyKeyPress will be true if didn´t passed through a check(bool var)
        if (!AnyKeyPress || millis() - timer > 75) {
            NextPress = false;
            PrevPress = false;
            UpPress = false;
            DownPress = false;
            SelPress = false;
            EscPress = false;
            AnyKeyPress = false;
            SerialCmdPress = false;
            NextPagePress = false;
            PrevPagePress = false;
            touchPoint.pressed = false;
            touchPoint.Clear();
#ifndef USE_TFT_eSPI_TOUCH
            InputHandler();
#endif
            timer = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
// Public Globals Variables
unsigned long previousMillis = millis();
int prog_handler; // 0 - Flash, 1 - LittleFS, 3 - Download
String cachedPassword = "";
int8_t interpreter_state = -1;
bool sdcardMounted = false;
bool gpsConnected = false;

// wifi globals
// TODO put in a namespace
bool wifiConnected = false;
bool isWebUIActive = false;
String wifiIP;

bool BLEConnected = false;
bool returnToMenu;
bool isSleeping = false;
bool isScreenOff = false;
bool dimmer = false;
char timeStr[16];
time_t localTime;
struct tm *timeInfo;
#if defined(HAS_RTC)
#if defined(HAS_RTC_PCF85063A)
pcf85063_RTC _rtc;
#else
cplus_RTC _rtc;
#endif
RTC_TimeTypeDef _time;
RTC_DateTypeDef _date;
bool clock_set = true;
#else
ESP32Time rtc;
bool clock_set = false;
#endif

std::vector<Option> options;
// Protected global variables
#if defined(HAS_SCREEN)
tft_logger tft = tft_logger(); // Invoke custom library
tft_sprite sprite = tft_sprite(&tft);
tft_sprite draw = tft_sprite(&tft);
volatile int tftWidth = TFT_HEIGHT;
#ifdef HAS_TOUCH
volatile int tftHeight =
    TFT_WIDTH - 20; // 20px to draw the TouchFooter(), were the btns are being read in touch devices.
#else
volatile int tftHeight = TFT_WIDTH;
#endif
#else
tft_logger tft;
SerialDisplayClass &sprite = tft;
SerialDisplayClass &draw = tft;
volatile int tftWidth = VECTOR_DISPLAY_DEFAULT_HEIGHT;
volatile int tftHeight = VECTOR_DISPLAY_DEFAULT_WIDTH;
#endif

#include "core/display.h"
#include "core/led_control.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/serialcmds.h"
#include "core/settings.h"
#include "core/wifi/webInterface.h"
#include "core/wifi/wifi_common.h"

// ── Compile-time WiFi config (edit include/MyWiFi.h) ──
#if __has_include("MyWiFi.h")
#include "MyWiFi.h"
#endif
#include <Wire.h>

/*********************************************************************
 **  Function: begin_storage
 **  Config LittleFS and SD storage
 *********************************************************************/
void begin_storage() {
    if (!LittleFS.begin(true)) { LittleFS.format(), LittleFS.begin(); }
    bool checkFS = setupSdCard();
    buttergotchiConfig.fromFile(checkFS);
    buttergotchiConfigPins.fromFile(checkFS);

    // ── Compile-time WiFi (edit include/MyWiFi.h) ──
#if defined(WIFI_SSID) && defined(WIFI_PASS)
    if (buttergotchiConfig.wifi.count(WIFI_SSID) == 0) {
        buttergotchiConfig.addWifiCredential(WIFI_SSID, WIFI_PASS);
    }
#endif
}

/*********************************************************************
 **  Function: _setup_gpio()
 **  Sets up a weak (empty) function to be replaced by /ports/* /interface.h
 *********************************************************************/
void _setup_gpio() __attribute__((weak));
void _setup_gpio() {}

/*********************************************************************
 **  Function: _post_setup_gpio()
 **  Sets up a weak (empty) function to be replaced by /ports/* /interface.h
 *********************************************************************/
void _post_setup_gpio() __attribute__((weak));
void _post_setup_gpio() {}

/*********************************************************************
 **  Function: setup_gpio
 **  Setup GPIO pins
 *********************************************************************/
void setup_gpio() {

    // init setup from /ports/*/interface.h
    _setup_gpio();

    // Smoochiee v2 uses a AW9325 tro control GPS, MIC, Vibro and CC1101 RX/TX powerlines
    ioExpander.init(IO_EXPANDER_ADDRESS, &Wire);

}

/*********************************************************************
 **  Function: begin_tft
 **  Config tft
 *********************************************************************/
void begin_tft() {
    tft.setRotation(buttergotchiConfigPins.rotation); // sometimes it misses the first command
    tft.invertDisplay(buttergotchiConfig.colorInverted);
    tft.setRotation(buttergotchiConfigPins.rotation);
    tftWidth = tft.width();
#ifdef HAS_TOUCH
    tftHeight = tft.height() - 20;
#else
    tftHeight = tft.height();
#endif
    resetTftDisplay();
    setBrightness(buttergotchiConfig.bright, false);
}

/*********************************************************************
 **  Function: boot_screen
 **  Draw boot screen
 *********************************************************************/
void boot_screen() {
    tft.setTextColor(buttergotchiConfig.priColor, buttergotchiConfig.bgColor);
    tft.setTextSize(FM);
    tft.drawPixel(0, 0, buttergotchiConfig.bgColor);
    tft.drawCentreString("Buttergotchi", tftWidth / 2, 10, 1);
    tft.setTextSize(FP);
    tft.drawCentreString(BUTTERGOTCHI_VERSION, tftWidth / 2, 25, 1);
}

/*********************************************************************
 **  Function: boot_screen_anim
 **  Draw boot screen
 *********************************************************************/
void boot_screen_anim() {
    boot_screen();
    int i = millis();
    int boot_img = 0;
    bool drawn = false;
    if (sdcardMounted) {
        if (SD.exists("/boot.jpg")) boot_img = 1;
        else if (SD.exists("/boot.gif")) boot_img = 3;
    }
    if (boot_img == 0 && LittleFS.exists("/boot.jpg")) boot_img = 2;
    else if (boot_img == 0 && LittleFS.exists("/boot.gif")) boot_img = 4;
    if (buttergotchiConfig.theme.boot_img) boot_img = 5;

    tft.drawPixel(0, 0, 0);
    while (millis() < i + 7000) {
        if ((millis() - i > 2000) && !drawn) {
            tft.fillScreen(buttergotchiConfig.bgColor);
            if (boot_img > 0) {
                drawImg(LittleFS, "/boot.jpg", 0, 0, true);
                tft.drawPixel(0, 0, 0);
            } else {
                int cx = tftWidth / 2;
                int cy = tftHeight / 2 + 6;
                tft.setTextDatum(MC_DATUM);
                tft.setTextSize(5);
                tft.setTextColor(buttergotchiConfig.priColor, buttergotchiConfig.bgColor);
                tft.drawString("B", cx, cy);
                tft.drawCircle(cx, cy, 20, buttergotchiConfig.priColor);
                int dotR = 2;
                int off = 24;
                tft.fillCircle(cx - off, cy - off, dotR, buttergotchiConfig.priColor);
                tft.fillCircle(cx + off, cy - off, dotR, buttergotchiConfig.priColor);
                tft.fillCircle(cx - off, cy + off, dotR, buttergotchiConfig.priColor);
                tft.fillCircle(cx + off, cy + off, dotR, buttergotchiConfig.priColor);
                tft.setTextDatum(TL_DATUM);
            }
            drawn = true;
        }
        if (check(AnyKeyPress)) {
            tft.fillScreen(buttergotchiConfig.bgColor);
            delay(10);
            return;
        }
    }

    // Clear splashscreen
    tft.fillScreen(buttergotchiConfig.bgColor);
}

/*********************************************************************
 **  Function: init_clock
 **  Clock initialisation for propper display in menu
 *********************************************************************/
void init_clock() {
#if defined(HAS_RTC)
    _rtc.begin();
#if defined(HAS_RTC_BM8563)
    _rtc.GetBm8563Time();
#endif
#if defined(HAS_RTC_PCF85063A)
    _rtc.GetPcf85063Time();
#endif
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
    struct tm timeinfo = {};
    timeinfo.tm_year = CURRENT_YEAR - 1900;
    timeinfo.tm_mon = 0x05;
    timeinfo.tm_mday = 0x14;
    time_t epoch = mktime(&timeinfo);
    rtc.setTime(epoch);
    clock_set = true;
    struct timeval tv = {.tv_sec = epoch};
    settimeofday(&tv, nullptr);
#endif
}

/*********************************************************************
 **  Function: init_led
 **  Led initialisation
 *********************************************************************/
void init_led() {
#ifdef HAS_RGB_LED
    beginLed();
#endif
}

/*********************************************************************
 **  Function: startup_sound
 **  Play sound or tone depending on device hardware
 *********************************************************************/
void startup_sound() {
    if (buttergotchiConfig.soundEnabled == 0) return; // if sound is disabled, do not play sound
#if !defined(LITE_VERSION)
#if defined(BUZZ_PIN)
    // Bip M5 just because it can. Does not bip if splashscreen is bypassed
    _tone(5000, 50);
    delay(200);
    _tone(5000, 50);
    /*  2fix: menu infinite loop */
#elif defined(HAS_NS4168_SPKR)
    // play a boot sound
    // playback removed - playAudioFile was deleted
#endif
#endif
}

/*********************************************************************
 **  Function: setup
 **  Where the devices are started and variables set
 *********************************************************************/
void setup() {

    // Create shared SPI bus mutex for CC_NRF_SPI (NRF24, CC1101, LoRa, W5500)
    cc_nrf_spi_mutex = xSemaphoreCreateMutex();

    Serial.setRxBufferSize(
        SAFE_STACK_BUFFER_SIZE / 4
    ); // Must be invoked before Serial.begin(). Default is 256 chars
    Serial.begin(115200);

    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    if (psramInit()) log_d("PSRAM Started");
    if (psramFound()) log_d("PSRAM Found");
    else log_d("PSRAM Not Found");
    log_d("Total PSRAM: %d", ESP.getPsramSize());
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    // declare variables
    prog_handler = 0;
    sdcardMounted = false;
    wifiConnected = false;
    BLEConnected = false;
    buttergotchiConfig.bright = 100; // theres is no value yet
    buttergotchiConfigPins.rotation = ROTATION;
    setup_gpio();
#if defined(HAS_SCREEN)
    tft.init();
    tft.setRotation(buttergotchiConfigPins.rotation);
    tft.fillScreen(TFT_BLACK);
    // buttergotchiConfig is not read yet.. just to show something on screen due to long boot time
    tft.setTextColor(0xFEC0, TFT_BLACK); // Buttergotchi Gold
    tft.drawCentreString("Booting", tft.width() / 2, tft.height() / 2, 1);
#else
    tft.begin();
#endif
    begin_storage();
    begin_tft();
    init_clock();
    init_led();

    options.reserve(20); // preallocate some options space to avoid fragmentation

    // Set WiFi country to avoid warnings and ensure max power
    const wifi_country_t country = {
        .cc = "US",
        .schan = 1,
        .nchan = 14,
#ifdef CONFIG_ESP_PHY_MAX_TX_POWER
        .max_tx_power = CONFIG_ESP_PHY_MAX_TX_POWER, // 20
#endif
        .policy = WIFI_COUNTRY_POLICY_MANUAL
    };

//     esp_wifi_set_max_tx_power(80); // 80 translates to 20dBm
//     esp_wifi_set_country(&country);

    // Some GPIO Settings (such as CYD's brightness control must be set after tft and sdcard)
    _post_setup_gpio();
    // Some board interfaces initialize or reset the backlight in post-setup,
    // so re-apply the stored brightness after that stage completes.
    setBrightness(buttergotchiConfig.bright, false);
    // end of post gpio begin

    // #ifndef USE_TFT_eSPI_TOUCH
    // This task keeps running all the time, will never stop
    xTaskCreate(
        taskInputHandler,              // Task function
        "InputHandler",                // Task Name
        INPUT_HANDLER_TASK_STACK_SIZE, // Stack size
        NULL,                          // Task parameters
        2,                             // Task priority (0 to 3), loopTask has priority 2.
        &xHandle                       // Task handle (not used)
    );
    // #endif
#if defined(HAS_SCREEN)
    buttergotchiConfig.openThemeFile(buttergotchiConfig.themeFS(), buttergotchiConfig.themePath, false);

    if (buttergotchiConfig.usbCompositeMode > 0) {
        USBComposite::begin();
    }

    if (!buttergotchiConfig.instantBoot) {
        boot_screen_anim();
        startup_sound();
    }
    if (buttergotchiConfig.wifiAtStartup) {
        log_i("Loading Wifi at Startup");
        xTaskCreate(
            wifiConnectTask,   // Task function
            "wifiConnectTask", // Task Name
            4096,              // Stack size
            NULL,              // Task parameters
            2,                 // Task priority (0 to 3), loopTask has priority 2.
            NULL               // Task handle (not used)
        );
    }
#endif
    //  start a task to handle serial commands while the webui is running
    startSerialCommandsHandlerTask(true);



    wakeUpScreen();
    if (buttergotchiConfig.startupApp != "" && !startupApp.startApp(buttergotchiConfig.startupApp)) {
        buttergotchiConfig.setStartupApp("");
    }
}

/**********************************************************************
 **  Function: loop
 **  Main loop
 **********************************************************************/
#if defined(HAS_SCREEN)
void loop() {

    tft.fillScreen(buttergotchiConfig.bgColor);

    mainMenu.begin();
    delay(1);
}
#else

void loop() {
    tft.setLogging();
    Serial.println(
        "\n"
        "██████  ██████  ██    ██  ██████ ███████ \n"
        "██   ██ ██   ██ ██    ██ ██      ██      \n"
        "██████  ██████  ██    ██ ██      █████   \n"
        "██   ██ ██   ██ ██    ██ ██      ██      \n"
        "██████  ██   ██  ██████   ██████ ███████ \n"
        "                                         \n"
        "         PREDATORY FIRMWARE\n\n"
        "Tips: Connect to the WebUI for better experience\n"
        "      Add your network by sending: wifi add ssid password\n\n"
        "At your command:"
    );

    // Enable navigation through webUI
    tft.fillScreen(buttergotchiConfig.bgColor);
    mainMenu.begin();
    vTaskDelay(10 / portTICK_PERIOD_MS);
}
#endif
