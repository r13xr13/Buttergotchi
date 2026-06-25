#include "wifi_commands.h"
#include "core/wifi/webInterface.h"
#include "core/wifi/wifi_common.h" //to return MAC addr
#include <globals.h>
#if !defined(LITE_VERSION)
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "modules/wifi/sniffer.h"
// #include "modules/wifi/responder.h"
#endif
uint32_t wifiCallback(cmd *c) {
    Command cmd(c);
    Argument statusArg = cmd.getArgument("status");
    String status = statusArg.getValue();
    status.trim();

    Argument ssidArg = cmd.getArgument("ssid");
    String ssid = ssidArg.getValue();
    ssid.trim();

    Argument pwdArg = cmd.getArgument("pwd");
    String pwd = pwdArg.getValue();
    pwd.trim();

    if (status == "off") {
        wifiDisconnect();
        return true;
    } else if (status == "on") {
        if (wifiConnected) {
            serialDevice->println("Wifi already connected");
            return true;
        }
        if (wifiConnecttoKnownNet()) return true;
        wifiDisconnect();
        return _setupAP();

    } else if (status == "add" && ssid != "" && pwd != "") {
        buttergotchiConfig.addWifiCredential(ssid, pwd);
        return true;
        } else if (status == "connect" && ssid != "") {
        String pwd = buttergotchiConfig.getWifiPassword(ssid);
        if (pwd == "") {
            serialDevice->println("ERROR: No saved password for SSID: " + ssid);
            return false;
        }
        if (_connectToWifiNetwork(ssid, pwd)) {
            wifiConnected = true;
            wifiIP = WiFi.localIP().toString();
            serialDevice->println("Connected to " + ssid + " OK");
        } else {
            wifiConnected = false;
            serialDevice->println("ERROR: Failed to connect to " + ssid);
        }
        return true;
    } else if (status == "list") {
        if (buttergotchiConfig.wifi.empty()) {
            serialDevice->println("No saved networks.");
        } else {
            serialDevice->println("Saved networks:");
            for (auto const &kv : buttergotchiConfig.wifi) {
                String marker = (WiFi.SSID() == kv.first && wifiConnected) ? " [CONNECTED]" : "";
                serialDevice->println("  " + kv.first + marker);
            }
        }
        return true;
    } else {
        serialDevice->println(
            "Invalid status: " + status +
            "\n"
            "Possible commands: \n"
            "-> wifi off (Disconnects Wifi)\n"
            "-> wifi on  (Connects to a known Wifi network. if there's no known network, starts in AP Mode)\n"
            "-> wifi add SSID Password (adds a network to the list)\n"
            "-> wifi connect SSID (connects to a specific saved network)\n"
            "-> wifi list (shows all saved networks)"
        );
        return false;
    }
}

uint32_t webuiCallback(cmd *c) {
    Command cmd(c);

    Argument arg = cmd.getArgument("noAp");
    bool noAp = arg.isSet();

    serialDevice->println(String("Starting Web UI ") + !noAp ? "AP" : "STA");
    serialDevice->println("Press ESC to quit");
    startWebUi(!noAp); // MEMO: will quit when check(EscPress)

    return true;
}
#if !defined(LITE_VERSION)
uint32_t snifferCallback(cmd *c) {
    sniffer_setup();

    return true;
}
#endif
/*
uint32_t responderCallback(cmd *c) {
    if (!wifiConnected) {
        Serial.println("Connect to a WiFi first.");
        return false;
    }

    responder();

    return true;
}
*/

void createWifiCommands(SimpleCLI *cli) {
    Command webuiCmd = cli->addCommand("webui", webuiCallback);
    webuiCmd.addFlagArg("noAp");

    Command wifiCmd = cli->addCommand("wifi", wifiCallback);
    wifiCmd.addPosArg("status");
    wifiCmd.addPosArg("ssid", "");
    wifiCmd.addPosArg("pwd", "");

#if !defined(LITE_VERSION)

    Command snifferCmd =
        cli->addCommand("sniffer", snifferCallback); // TODO: be able to exit from it from Serial

#endif
    // Command responderCmd = cli->addCommand("responder", responderCallback); TODO
}
