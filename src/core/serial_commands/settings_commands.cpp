#include "settings_commands.h"
#include <globals.h>

uint32_t settingsCallback(cmd *c) {
    Command cmd(c);

    Argument setting_name_arg = cmd.getArgument("setting_name");
    Argument setting_value_arg = cmd.getArgument("setting_value");
    String setting_name = setting_name_arg.getValue();
    String setting_value = setting_value_arg.getValue();
    setting_name.trim();
    setting_value.trim();

    JsonDocument jsonDoc = buttergotchiConfig.toJson();
    JsonObject setting = jsonDoc.as<JsonObject>();

    if (setting_name.length() == 0 && setting_value.length() == 0) {
        // no args, just prints current config
        serializeJsonPretty(jsonDoc, Serial);
        serialDevice->println("");
        return true;
    }

    if (setting[setting_name].isNull()) {
        serialDevice->println("Invalid field name: " + setting_name);
        return false;
    }

    if (setting_value.length() == 0) {
        serialDevice->print(setting_name + " = ");
        serialDevice->println(setting[setting_name].as<String>());
        return true;
    }

    // TODO: improve this logic and move to ButtergotchiConfig
    if (setting_name == "priColor") buttergotchiConfig.setUiColor(setting_value.toInt());
    if (setting_name == "rot") buttergotchiConfigPins.setRotation(setting_value.toInt());
    if (setting_name == "dimmerSet") buttergotchiConfig.setDimmer(setting_value.toInt());
    if (setting_name == "bright") buttergotchiConfig.setBright(setting_value.toInt());
    if (setting_name == "tmz") buttergotchiConfig.setTmz(setting_value.toFloat());
    if (setting_name == "soundEnabled") buttergotchiConfig.setSoundEnabled(setting_value.toInt());
    if (setting_name == "wifiAtStartup") buttergotchiConfig.setWifiAtStartup(setting_value.toInt());
    if (setting_name == "webUI") {
        buttergotchiConfig.setWebUICreds(
            setting_value.substring(0, setting_value.indexOf(",")),
            setting_value.substring(setting_value.indexOf(",") + 1)
        );
    }
    if (setting_name == "wifiAp") {
        buttergotchiConfig.setWifiApCreds(
            setting_value.substring(0, setting_value.indexOf(",")),
            setting_value.substring(setting_value.indexOf(",") + 1)
        );
    }
    if (setting_name == "wifi") {
        buttergotchiConfig.addWifiCredential(
            setting_value.substring(0, setting_value.indexOf(",")),
            setting_value.substring(setting_value.indexOf(",") + 1)
        );
    }
    if (setting_name == "bleName") buttergotchiConfigPins.setBleName(setting_value);
    if (setting_name == "irTx") buttergotchiConfigPins.setIrTxPin(setting_value.toInt());
    if (setting_name == "irTxRepeats")
        buttergotchiConfigPins.setIrTxRepeats(static_cast<uint8_t>(setting_value.toInt()));
    if (setting_name == "irRx") buttergotchiConfigPins.setIrRxPin(setting_value.toInt());
    if (setting_name == "rfTx") buttergotchiConfigPins.setRfTxPin(setting_value.toInt());
    if (setting_name == "rfRx") buttergotchiConfigPins.setRfRxPin(setting_value.toInt());
    if (setting_name == "rfModule")
        buttergotchiConfigPins.setRfModule(static_cast<RFModules>(setting_value.toInt()));
    if (setting_name == "rfFreq" && setting_value.toFloat())
        buttergotchiConfigPins.setRfFreq(setting_value.toFloat());
    if (setting_name == "rfFxdFreq") buttergotchiConfigPins.setRfFxdFreq(setting_value.toInt());
    if (setting_name == "rfScanRange") buttergotchiConfigPins.setRfScanRange(setting_value.toInt());
    if (setting_name == "rfidModule")
        buttergotchiConfigPins.setRfidModule(static_cast<RFIDModules>(setting_value.toInt()));
    if (setting_name == "wigleBasicToken") buttergotchiConfig.setWigleBasicToken(setting_value);
    if (setting_name == "wdgwarsApiKey") buttergotchiConfig.setWdgwarsApiKey(setting_value);
    if (setting_name == "devMode") buttergotchiConfig.setDevMode(setting_value.toInt());
    if (setting_name == "disabledMenus") buttergotchiConfig.addDisabledMenu(setting_value);

    return true;
}

uint32_t factoryResetCallback(cmd *c) {
    buttergotchiConfig.factoryReset();
    serialDevice->println("Factory reset done");
    return true;
}

void createSettingsCommands(SimpleCLI *cli) {
    cli->addCommand("factory_reset", factoryResetCallback);

    Command cmd = cli->addCommand("set/tings", settingsCallback);
    cmd.addPosArg("setting_name", "");
    cmd.addPosArg("setting_value", "");
}
