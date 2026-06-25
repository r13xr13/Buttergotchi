#include "usb_composite.h"
#include <globals.h>

// TinyUSB is only available on ESP32-S2/S3/C3 and similar SoCs with
// built-in USB-OTG. Older ESP32 chips (CYD, CPlus2, C5) lack the
// arduino-esp32 TinyUSB layer, so we provide a no-op stub for them.
#if __has_include(<tusb.h>)
#include <USB.h>
#include <USBHID.h>
#include <tusb.h>

// Static USBHID instance to register the HID interface with TinyUSB at boot.
// This lets later HID sub-devices (keyboard, mouse, U2F) attach without
// creating their own USBHID instance. The USBHID constructor only registers
// once (guarded by tinyusb_hid_devices_is_initialized).
static USBHID _usbhid(HID_ITF_PROTOCOL_NONE);

static bool _active = false;
static bool _enumerated = false;

bool USBComposite::begin() {
    if (_active) return true;

    USB.begin();
    _usbhid.begin();
    _active = true;

    if (buttergotchiConfig.usbCompositeMode == USB_COMPOSITE_AUTO) {
        // Wait up to 1s for a USB host to enumerate
        uint32_t start = millis();
        while (millis() - start < 1000) {
            if (tud_ready()) {
                _enumerated = true;
                break;
            }
            delay(10);
        }

        if (!_enumerated) {
            // No host detected — mark inactive so original USB.begin() calls
            // can still work. The USB peripheral stays powered but idle.
            _active = false;
            return false;
        }
    }

    if (buttergotchiConfig.usbCompositeMode == USB_COMPOSITE_ON) {
        _enumerated = true;
    }

    return _enumerated;
}

void USBComposite::end() {
    // No clean way to fully tear down the USB stack without affecting
    // Serial (CDC). Setting _active = false lets existing feature code
    // fall through to their own USB.begin() if needed.
    _active = false;
    _enumerated = false;
}

bool USBComposite::isActive() { return _active; }

bool USBComposite::isConnected() { return _enumerated && _active && tud_ready(); }

#else // !__has_include(<tusb.h>)

// No-op stub for boards without TinyUSB support.
bool USBComposite::begin() { return false; }
void USBComposite::end() {}
bool USBComposite::isActive() { return false; }
bool USBComposite::isConnected() { return false; }

#endif // __has_include(<tusb.h>)

bool USBComposite::shouldActivate() {
    return buttergotchiConfig.usbCompositeMode != USB_COMPOSITE_OFF;
}
