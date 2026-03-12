#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "parser.h"

static HWData hwData;
static unsigned long lastDataTime = 0;
static const unsigned long TIMEOUT_MS = 5000;  // 5 sec without data = disconnected
static char jsonBuf[2048];

void setup() {
    Serial.begin(SERIAL_BAUD);
    printf("\n=== PC Hardware Monitor for WT32-SC01 ===\n");

    // Initialize hardware data with defaults
    memset(&hwData, 0, sizeof(hwData));
    hwData.fan[0] = hwData.fan[1] = -1;
    hwData.gpu_fan_rpm = -1;
    strncpy(hwData.cpu_name, "---", sizeof(hwData.cpu_name));
    strncpy(hwData.gpu_name, "---", sizeof(hwData.gpu_name));
    hwData.connected = false;

    display.init();
    display.showWaiting();

    printf("Ready. Waiting for serial data...\n");
}

void loop() {
    // Try to read a complete JSON line from serial
    if (serial_readLine(jsonBuf, sizeof(jsonBuf))) {
        if (parseHWData(jsonBuf, hwData)) {
            lastDataTime = millis();
            display.update(hwData);
        }
    }

    // Handle touch input
    display.handleTouch(hwData);

    // Check for timeout
    if (hwData.connected && (millis() - lastDataTime > TIMEOUT_MS)) {
        hwData.connected = false;
        display.showWaiting();
        printf("Connection lost - no data for %lu ms\n", TIMEOUT_MS);
    }

    delay(10);  // Small delay to prevent busy-loop
}
