# Buttergotchi

A pwnagotchi-inspired WiFi handshake capture tool for ESP32 devices. Supports a wide range of ESP32-based boards.

Buttergotchi advertises its presence on WiFi channels, discovers other Buttergotchi and pwnagotchi devices, sends deauth frames to force client reconnections, and captures WPA handshake PCAP files for offline cracking.

## Features

- Handshake capture via promiscuous WiFi sniffing
- Automatic channel hopping with active deauthentication
- Peer discovery (pwnagotchi-compatible advertising protocol)
- Mood system with context-responsive face and phrase
- Handshake toast notifications on capture
- Deauth whitelist (MAC and SSID filtering)
- All-channels mode toggle
- Session logging to SD card (CSV)
- PCAP output to SD card or LittleFS

## Supported Boards

Buttergotchi supports 50+ ESP32-based board configurations:

### M5Stack
- Cardputer
- StickS3
- StickC Plus (Plus1.1) / Plus2
- Core2 / CoreS3 / Core (4MB) / Core (16MB)
- DinMeter

### LilyGo
- T-Deck / T-Deck Pro
- T-Embed / T-Embed CC1101
- T-Display-S3 / S3 Pro / S3 Touch / S3 MMC
- T-Watch-S3
- T-HMI
- T-LoRa-Pager
- T-Display-TTGo

### CYD (Cheap Yellow Display)
- 2432S028 (2.8" resistive touch)
- 2USB
- 2432W328C / 2432W328C_2
- 2432W328R / S024R
- 3248S035R / 3248S035C

### Marauder
- Mini / v7 / V4-V6 / v61

### Elecrow
- 24B / 28B / 35B / 35Bv2_2

### ESP32 DevKits
- ESP32-S3-DevKitC-1 (PSRAM / no PSRAM)
- ESP32-C5 / C5-TFT
- ESP-General

### Other
- Arduino Nesso N1
- Smoochiee Board
- XK404
- Reaper
- Phantom S024R
- Awok Mini / Awok Touch
- NM-CYD-C5
- ES3C28P
- WaveSentry-R1

## Releases

Pre-built binaries are available for the most common boards. Flash with esptool:

```bash
esptool.py --port /dev/ttyACM0 --baud 921600 write_flash 0x00000 releases/Buttergotchi-m5stack-cardputer.bin
```

### Available Binaries

| Board | File |
|---|---|
| M5Stack Cardputer | `Buttergotchi-m5stack-cardputer.bin` |
| M5Stack StickS3 | `Buttergotchi-m5stack-sticks3.bin` |
| LilyGo T-Deck | `Buttergotchi-lilygo-t-deck.bin` |
| LilyGo T-Embed CC1101 | `Buttergotchi-lilygo-t-embed-cc1101.bin` |
| CYD-2432S028 | `Buttergotchi-CYD-2432S028.bin` |

Don't see your board? Build from source — see below.

## Building

### Prerequisites

- PlatformIO CLI or VS Code extension
- Git

### Build and Flash

```bash
git clone https://github.com/r13xr13/Buttergotchi.git
cd Buttergotchi

# Build for your board
pio run -e m5stack-cardputer

# Build and flash
pio run -e m5stack-cardputer -t upload

# Build filesystem (LittleFS) for your board
pio run -e m5stack-cardputer -t uploadfs
```

### Selecting a Board

Edit `platformio.ini` and change the `default_envs` list, or pass `-e` to each command:

```bash
pio run -e m5stack-sticks3 -t upload
pio run -e CYD-2432S028 -t upload
pio run -e lilygo-t-deck -t upload
```

For boards with no SD card slot, handshake PCAPs are stored in LittleFS.

## Usage

1. Flash the firmware to your device
2. Boot the device -- Buttergotchi starts automatically as the main menu entry
3. Select "Buttergotchi" from the main menu
4. The device begins channel hopping, advertising, and capturing handshakes

### In-App Display

- **Top bar**: Current channel (CH), handshake count (HS), uptime, battery level
- **Mood area**: Face expression and context-aware phrase
- **Bottom bar**: Buttergotchi branding and active peer count
- **Friend panel**: Up to 3 visible peers with RSSI signal bars

### Controls

Press the button/enter key to open the in-app menu:
- Find friends (refresh peer discovery)
- All Ch: ON/OFF (toggle all 12 channels vs priority 1/6/11)
- Whitelist (manage deauth MAC/SSID exclusions)
- Main Menu (exit Buttergotchi)

## Configuration

Theme colors, brightness, and other settings are stored on LittleFS or SD card. The default theme uses Buttergotchi gold accent colors.

### Deauth Whitelist

Add MAC addresses or SSID names to the whitelist to prevent deauth attacks against specific networks.

## File Locations

Handshake PCAPs are saved to:
- SD card: `/ButtergotchiPCAP/handshakes/HS_<MAC>_<SSID>.pcap`
- LittleFS: `/ButtergotchiPCAP/handshakes/HS_<MAC>_<SSID>.pcap`

Session logs (when SD is available):
- SD card: `/ButtergotchiPCAP/sessions.csv`

## Technical Details

- Sniffer mode: EAPOL/Handshakes only
- Channel dwell: 1500ms per channel
- Deauth interval: 300ms with 5-frame burst
- Default channels: 1, 6, 11 (priority)
- All channels mode: 1-12
- Peer discovery: pwnagotchi-compatible beacon parsing
- Advertising: 802.11 beacon frames with JSON payload in custom IE

## Credits

- Pwnagotchi protocol compatibility from the original pwnagotchi project (evilsocket)
- Pwngrid advertising and sniffer logic adapted from 7h30th3r0n3 and viniciusbo
- Thanks to bmorcelli for ESP32 framework and library support

## License

GNU General Public License v3.0
