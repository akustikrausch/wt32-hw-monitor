#include "parser.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

static char lineBuf[4096];
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

    // Time sync
    data.pc_timestamp = doc["ts"] | 0UL;
    data.tz_offset = doc["tzo"] | 3600;

    // Main screen data
    data.cpu_load = doc["cpu"] | 0.0f;
    data.gpu_load = doc["gpuload"] | 0.0f;
    data.cpu_temp = doc["cputemp"] | 0.0f;
    data.gpu_temp = doc["gputemp"] | 0.0f;
    data.cpu_clock = doc["cpuclk"] | 0.0f;
    data.cpu_power = doc["cpupwr"] | 0.0f;
    data.cpu_voltage = doc["cpuvolt"] | 0.0f;
    data.gpu_vram_used = doc["gpuvram"] | 0.0f;
    data.gpu_vram_total = doc["gpuvtot"] | 0.0f;
    data.ram_percent = doc["ram"] | 0.0f;
    data.ram_used_gb = doc["ramused"] | 0.0f;
    data.ram_total_gb = doc["ramtotal"] | 0.0f;

    data.fan[0] = doc["fan1"] | -1;
    data.fan[1] = doc["fan2"] | -1;

    data.storage_total_tb = doc["stotal"] | 0.0f;
    data.storage_used_tb = doc["sused"] | 0.0f;
    data.storage_free_tb = doc["sfree"] | 0.0f;

    // Network
    data.net_download = doc["netdl"] | 0.0f;
    data.net_upload = doc["netul"] | 0.0f;
    data.net_util = doc["netutil"] | 0.0f;
    data.net_data_up = doc["netdup"] | 0.0f;
    data.net_data_dl = doc["netddl"] | 0.0f;
    const char *netName = doc["netname"] | "Unknown";
    strncpy(data.net_adapter, netName, sizeof(data.net_adapter) - 1);
    data.net_adapter[sizeof(data.net_adapter) - 1] = '\0';

    // GPU detail
    data.gpu_core_clock = doc["gpuclk"] | 0.0f;
    data.gpu_mem_clock = doc["gpumclk"] | 0.0f;
    data.gpu_power = doc["gpupwr"] | 0.0f;
    data.gpu_hotspot = doc["gpuhs"] | 0.0f;
    data.gpu_fan_rpm = doc["gpufan"] | -1;

    // Disk details
    JsonArray dt = doc["dtemp"];
    JsonArray dn = doc["dname"];
    JsonArray ds = doc["dsize"];
    data.disk_count = 0;
    if (dt) {
        for (int i = 0; i < 8 && i < (int)dt.size(); i++) {
            data.disk_temp[i] = dt[i] | -1;
            const char *name = (dn && i < (int)dn.size()) ? (dn[i] | "?") : "?";
            strncpy(data.disk_name[i], name, 11);
            data.disk_name[i][11] = '\0';
            data.disk_size_gb[i] = (ds && i < (int)ds.size()) ? (ds[i] | 0) : 0;
            data.disk_count++;
        }
    }
    for (int i = data.disk_count; i < 8; i++) {
        data.disk_temp[i] = -1;
        data.disk_name[i][0] = '\0';
        data.disk_size_gb[i] = 0;
    }

    // CPU per-core loads
    JsonArray cc = doc["ccores"];
    data.cpu_core_count = 0;
    if (cc) {
        for (int i = 0; i < 16 && i < (int)cc.size(); i++) {
            data.cpu_core_load[i] = cc[i] | 0.0f;
            data.cpu_core_count++;
        }
    }
    for (int i = data.cpu_core_count; i < 16; i++) {
        data.cpu_core_load[i] = 0.0f;
    }

    const char *cpuName = doc["cpuname"] | "Unknown CPU";
    const char *gpuName = doc["gpuname"] | "Unknown GPU";
    strncpy(data.cpu_name, cpuName, sizeof(data.cpu_name) - 1);
    data.cpu_name[sizeof(data.cpu_name) - 1] = '\0';
    strncpy(data.gpu_name, gpuName, sizeof(data.gpu_name) - 1);
    data.gpu_name[sizeof(data.gpu_name) - 1] = '\0';

    // Advanced: Motherboard
    data.mb_vcore = doc["mbvc"] | 0.0f;
    data.mb_33v = doc["mb33"] | 0.0f;
    data.mb_cmos = doc["mbcm"] | 0.0f;

    JsonArray mbtp = doc["mbtp"];
    JsonArray mbtn = doc["mbtn"];
    data.mb_temp_count = 0;
    if (mbtp) {
        for (int i = 0; i < 6 && i < (int)mbtp.size(); i++) {
            data.mb_temps[i] = mbtp[i] | 0;
            const char *tn = (mbtn && i < (int)mbtn.size()) ? (mbtn[i] | "?") : "?";
            strncpy(data.mb_tnames[i], tn, 8);
            data.mb_tnames[i][8] = '\0';
            data.mb_temp_count++;
        }
    }
    for (int i = data.mb_temp_count; i < 6; i++) {
        data.mb_temps[i] = 0;
        data.mb_tnames[i][0] = '\0';
    }

    JsonArray mbfc = doc["mbfc"];
    data.mb_fan_ctrl_count = 0;
    if (mbfc) {
        for (int i = 0; i < 4 && i < (int)mbfc.size(); i++) {
            data.mb_fan_ctrl[i] = mbfc[i] | 0;
            data.mb_fan_ctrl_count++;
        }
    }
    for (int i = data.mb_fan_ctrl_count; i < 4; i++) {
        data.mb_fan_ctrl[i] = 0;
    }

    // Advanced: CPU
    data.cpu_soc_temp = doc["csoc"] | 0.0f;
    data.cpu_ccd1_temp = doc["ccd1"] | 0.0f;
    data.cpu_ccd2_temp = doc["ccd2"] | 0.0f;
    data.cpu_tdc = doc["ctdc"] | 0.0f;
    data.cpu_fabric_clk = doc["cfab"] | 0.0f;
    data.cpu_bus_speed = doc["cbus"] | 0.0f;
    data.cpu_mem_clk = doc["cmcl"] | 0.0f;

    // Advanced: GPU
    data.gpu_volt = doc["gvlt"] | 0.0f;
    data.gpu_mem_ctrl_load = doc["gmcl"] | 0.0f;
    data.gpu_vid_eng_load = doc["gvel"] | 0.0f;
    data.gpu_bus_load = doc["gbl"] | 0.0f;
    data.gpu_board_pwr = doc["gbpw"] | 0.0f;
    data.gpu_fan_ctrl = doc["gfct"] | 0.0f;
    data.gpu_pcie_rx = doc["gprx"] | 0.0f;
    data.gpu_pcie_tx = doc["gptx"] | 0.0f;
    data.gpu_d3d_3d = doc["gd3d"] | 0.0f;
    data.gpu_d3d_copy = doc["gdcp"] | 0.0f;
    data.gpu_d3d_vdec = doc["gdvd"] | 0.0f;
    data.gpu_d3d_venc = doc["gdve"] | 0.0f;
    data.gpu_dmem = doc["gdm"] | 0.0f;
    data.gpu_smem = doc["gsm"] | 0.0f;

    // Advanced: RAM
    JsonArray dimt = doc["dimt"];
    data.dimm_count = 0;
    if (dimt) {
        for (int i = 0; i < 4 && i < (int)dimt.size(); i++) {
            data.dimm_temps[i] = dimt[i] | 0;
            data.dimm_count++;
        }
    }
    for (int i = data.dimm_count; i < 4; i++) {
        data.dimm_temps[i] = 0;
    }
    data.vm_used = doc["vmu"] | 0.0f;
    data.vm_total = doc["vmt"] | 0.0f;

    // Advanced: Disk I/O
    JsonArray drd = doc["drd"];
    JsonArray dwr = doc["dwr"];
    JsonArray dact = doc["dact"];
    for (int i = 0; i < 8; i++) {
        data.disk_read[i] = (drd && i < (int)drd.size()) ? (drd[i] | 0.0f) : 0.0f;
        data.disk_write[i] = (dwr && i < (int)dwr.size()) ? (dwr[i] | 0.0f) : 0.0f;
        data.disk_act[i] = (dact && i < (int)dact.size()) ? (dact[i] | 0.0f) : 0.0f;
    }

    data.connected = true;
    return true;
}
