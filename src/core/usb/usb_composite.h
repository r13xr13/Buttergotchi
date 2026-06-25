#ifndef BUTTERGOTCHI_USB_COMPOSITE_H
#define BUTTERGOTCHI_USB_COMPOSITE_H

#include <Arduino.h>

// USB Composite mode
// 0 = Off (original per-feature USB.begin() behavior preserved)
// 1 = On  (force composite init at boot)
// 2 = Auto (init composite, tear down if no host enumerates within timeout)
enum USBCompositeMode {
    USB_COMPOSITE_OFF  = 0,
    USB_COMPOSITE_ON   = 1,
    USB_COMPOSITE_AUTO = 2,
};

class USBComposite {
public:
    // Initialize composite USB — calls USB.begin() + USBHID::begin() once
    // Returns true if USB host enumerated (or mode is ON)
    static bool begin();

    // Tear down composite USB — allows original per-feature USB.begin() to work
    static void end();

    // Check if composite USB is currently active
    static bool isActive();

    // Check if a USB host has enumerated (VBUS + configuration received)
    static bool isConnected();

    // Convenience — returns true if USB host is connected OR mode is ON
    static bool shouldActivate();
};

#endif
