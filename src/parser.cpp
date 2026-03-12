#include "parser.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

static char lineBuf[512];
static int linePos = 0;

bool serial_readLine(char *buf, int maxLen) {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (linePos > 0) {
                lineBuf[linePos] = '\0';
                strncpy(buf, lineBuf, maxLen - 1);
                buf[maxLen - 1] = '\0';
                linePos = 0;
                return true;
            }
        } else {
            if (linePos < (int)sizeof(lineBuf) - 1) {
                lineBuf[linePos++] = c;
            }
        }
    }
    return false;
}

bool parseHWData(const char *json, HWData &data) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        printf("JSON parse error: %s\n", err.c_str());
        return false;
    }

    data.cpu_load = doc["cpu"] | 0.0f;
    data.gpu_load = doc["gpuload"] | 0.0f;
    data.cpu_temp = doc["cputemp"] | 0.0f;
    data.gpu_temp = doc["gputemp"] | 0.0f;
    data.ram_percent = doc["ram"] | 0.0f;
    data.ram_used_gb = doc["ramused"] | 0.0f;
    data.ram_total_gb = doc["ramtotal"] | 0.0f;

    data.fan[0] = doc["fan1"] | -1;
    data.fan[1] = doc["fan2"] | -1;
    data.fan[2] = doc["fan3"] | -1;
    data.fan[3] = doc["fan4"] | -1;

    data.storage_total_tb = doc["stotal"] | 0.0f;
    data.storage_used_tb = doc["sused"] | 0.0f;
    data.storage_free_tb = doc["sfree"] | 0.0f;

    const char *cpuName = doc["cpuname"] | "Unknown CPU";
    const char *gpuName = doc["gpuname"] | "Unknown GPU";
    strncpy(data.cpu_name, cpuName, sizeof(data.cpu_name) - 1);
    data.cpu_name[sizeof(data.cpu_name) - 1] = '\0';
    strncpy(data.gpu_name, gpuName, sizeof(data.gpu_name) - 1);
    data.gpu_name[sizeof(data.gpu_name) - 1] = '\0';

    data.connected = true;
    return true;
}
