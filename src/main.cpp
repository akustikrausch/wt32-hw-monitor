#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "parser.h"

static HWData hwData;
static ProcList procList;
static unsigned long lastLinkTime = 0;   // last valid message of any kind from the PC
static const unsigned long LINK_TIMEOUT_MS = 5000;  // 5 sec without any message = PC gone
static char jsonBuf[4096];

void setup() {
    // Default RX buffer is only 256 bytes (~22 ms at 115200 baud). A screen redraw takes
    // longer than that, so bytes of an incoming line were lost whenever the two overlapped.
    Serial.setRxBufferSize(sizeof(jsonBuf));
    Serial.begin(SERIAL_BAUD);
    printf("\n=== PC Hardware Monitor for WT32-SC01 ===\n");

    // Initialize hardware data with defaults
    memset(&hwData, 0, sizeof(hwData));
    hwData.fan[0] = hwData.fan[1] = -1;
    hwData.gpu_fan_rpm = -1;
    strncpy(hwData.cpu_name, "---", sizeof(hwData.cpu_name));
    strncpy(hwData.gpu_name, "---", sizeof(hwData.gpu_name));

    display.init();
    display.setProcList(&procList);
    display.showStandby(STANDBY_NO_PC);

    printf("Ready. Waiting for serial data...\n");
}

void loop() {
    // Try to read a complete JSON line from serial
    if (serial_readLine(jsonBuf, sizeof(jsonBuf))) {
        MsgType msg = parseMessage(jsonBuf, hwData, procList);
        if (msg != MSG_INVALID) {
            lastLinkTime = millis();
            display.syncTime(hwData.pc_timestamp, hwData.tz_offset);
        }

        if (msg == MSG_DATA) {
            display.update(hwData);
        } else if (msg == MSG_PROCS) {
            display.updateProcs();
        } else if (msg == MSG_HEARTBEAT) {
            // The script only sends heartbeats once LibreHardwareMonitor has been silent for a while
            if (display.isStandby()) {
                display.setStandbyReason(STANDBY_NO_LHM);
            } else {
                display.showStandby(STANDBY_NO_LHM);
                printf("LibreHardwareMonitor not answering\n");
            }
        }
    }

    // Handle touch input
    display.handleTouch(hwData);

    // Nothing at all from the PC for a while
    if (millis() - lastLinkTime > LINK_TIMEOUT_MS) {
        if (!display.isStandby()) {
            display.showStandby(STANDBY_NO_PC);
            printf("Connection lost - no data for %lu ms\n", LINK_TIMEOUT_MS);
        } else {
            display.setStandbyReason(STANDBY_NO_PC);
        }
    }

    // Update standby clock display
    if (display.isStandby()) {
        display.updateStandby();
    }

    delay(10);  // Small delay to prevent busy-loop
}
