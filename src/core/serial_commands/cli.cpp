#include "cli.h"
#include "core/sd_functions.h"
#include "gpio_commands.h"
#include "power_commands.h"
#include "screen_commands.h"
#include "settings_commands.h"
#include "storage_commands.h"
#include "wifi_commands.h"
#include <globals.h>

void cliErrorCallback(cmd_error *e) {
    CommandError cmdError(e); // Create wrapper object

    serialDevice->print("ERROR: ");
    serialDevice->println(cmdError.toString());

    if (cmdError.hasCommand()) {
        serialDevice->print("Did you mean \"");
        serialDevice->print(cmdError.getCommand().toString());
        serialDevice->println("\"?");
    }
}

SerialCli::SerialCli() { setup(); }

void SerialCli::setup() {
    _cli.setOnError(cliErrorCallback);

    createGpioCommands(&_cli);
    createPowerCommands(&_cli);
    createSettingsCommands(&_cli);
    createStorageCommands(&_cli);
    createWifiCommands(&_cli);

#ifdef HAS_SCREEN
    createScreenCommands(&_cli);
#endif
}
