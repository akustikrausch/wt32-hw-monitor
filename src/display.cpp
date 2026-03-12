#include "display.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

// === LovyanGFX ST7796S configuration for WT32-SC01 ===
class LGFX_WT32 : public lgfx::LGFX_Device {
    lgfx::Panel_ST7796 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Light_PWM _light;
    lgfx::Touch_FT5x06 _touch;
public:
    LGFX_WT32() {
        auto bcfg = _bus.config();
        bcfg.spi_host = HSPI_HOST;
        bcfg.spi_mode = 0;
        bcfg.freq_write = 40000000;
        bcfg.freq_read = 16000000;
        bcfg.pin_sclk = TFT_SCLK;
        bcfg.pin_mosi = TFT_MOSI;
        bcfg.pin_miso = -1;
        bcfg.pin_dc = TFT_DC;
        _bus.config(bcfg);
        _panel.setBus(&_bus);

        auto pcfg = _panel.config();
        pcfg.pin_cs = TFT_CS;
        pcfg.pin_rst = TFT_RST;
        pcfg.pin_busy = -1;
        pcfg.panel_width = 320;
        pcfg.panel_height = 480;
        pcfg.offset_x = 0;
        pcfg.offset_y = 0;
        pcfg.offset_rotation = 0;
        pcfg.readable = false;
        pcfg.invert = false;
        pcfg.rgb_order = false;
        pcfg.memory_width = 320;
        pcfg.memory_height = 480;
        _panel.config(pcfg);

        auto lcfg = _light.config();
        lcfg.pin_bl = TFT_BL;
        lcfg.invert = false;
        lcfg.freq = 44100;
        lcfg.pwm_channel = 7;
        _light.config(lcfg);
        _panel.setLight(&_light);

        auto tcfg = _touch.config();
        tcfg.i2c_port = 1;
        tcfg.i2c_addr = 0x38;
        tcfg.pin_sda = TOUCH_SDA;
        tcfg.pin_scl = TOUCH_SCL;
        tcfg.freq = 400000;
        tcfg.x_min = 0;
        tcfg.x_max = 319;
        tcfg.y_min = 0;
        tcfg.y_max = 479;
        _touch.config(tcfg);
        _panel.setTouch(&_touch);

        setPanel(&_panel);
    }
};

static LGFX_WT32 lcd;
PCMonitorDisplay display;

void PCMonitorDisplay::init() {
    _lcd = &lcd;
    _sprite = nullptr;
    _history_idx = 0;
    _first_draw = true;
    memset(_cpu_history, 0, sizeof(_cpu_history));
    memset(_gpu_history, 0, sizeof(_gpu_history));

    lcd.init();
    lcd.setRotation(1);
    lcd.setBrightness(200);
    lcd.fillScreen(COL_BG);

    printf("Display initialized (480x320, no header, direct draw)\n");
    printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

uint16_t PCMonitorDisplay::tempColor(float temp) {
    if (temp < TEMP_WARN) return COL_GREEN;
    if (temp < TEMP_CRIT) return COL_YELLOW;
    return COL_RED;
}

void PCMonitorDisplay::drawBar(int x, int y, int w, int h, float percent, uint16_t color) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int filled = (int)(w * percent / 100.0f);
    if (filled > 0) _lcd->fillRect(x, y, filled, h, color);
    if (filled < w) _lcd->fillRect(x + filled, y, w - filled, h, COL_BAR_BG);
    _lcd->drawRect(x, y, w, h, COL_DIVIDER);
}

void PCMonitorDisplay::drawGraph(int x, int y, int w, int h, float *history, int len, uint16_t color) {
    _lcd->fillRect(x + 1, y + 1, w - 2, h - 2, COL_BAR_BG);
    _lcd->drawRect(x, y, w, h, COL_DIVIDER);

    // Grid lines at 25%, 50%, 75%
    for (int g = 1; g <= 3; g++) {
        int gy = y + h - (h * g / 4);
        for (int gx = x + 2; gx < x + w - 2; gx += 4)
            _lcd->drawPixel(gx, gy, COL_DIVIDER);
    }

    if (len < 2) return;
    for (int i = 1; i < len; i++) {
        int idx_prev = (_history_idx + i - 1) % len;
        int idx_curr = (_history_idx + i) % len;
        int x0 = x + (i - 1) * (w - 4) / (len - 1) + 2;
        int x1 = x + i * (w - 4) / (len - 1) + 2;
        int y0 = y + h - 2 - (int)(history[idx_prev] * (h - 4) / 100.0f);
        int y1 = y + h - 2 - (int)(history[idx_curr] * (h - 4) / 100.0f);
        if (y0 < y + 1) y0 = y + 1;
        if (y1 < y + 1) y1 = y + 1;
        if (y0 > y + h - 2) y0 = y + h - 2;
        if (y1 > y + h - 2) y1 = y + h - 2;
        _lcd->drawLine(x0, y0, x1, y1, color);
    }
}

void PCMonitorDisplay::showWaiting() {
    _lcd->fillScreen(COL_BG);
    _lcd->setFont(&fonts::Font4);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawCenterString("Waiting for PC...", SCREEN_W / 2, 120);
    _lcd->setFont(&fonts::Font2);
    _lcd->drawCenterString("Start LibreHardwareMonitor + pc_monitor.py", SCREEN_W / 2, 160);
}

void PCMonitorDisplay::update(const HWData &data) {
    char buf[64];

    // === First draw: clear + static elements ===
    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        _lcd->drawFastVLine(COL_RIGHT_X - 2, CPU_GPU_Y, CPU_GPU_H, COL_DIVIDER);
        _lcd->drawFastHLine(4, RAM_Y, SCREEN_W - 8, COL_DIVIDER);
        _lcd->drawFastHLine(4, STORAGE_Y, SCREEN_W - 8, COL_DIVIDER);
        _lcd->drawFastHLine(4, BOTTOM_Y, SCREEN_W - 8, COL_DIVIDER);

        // CPU/GPU names (static)
        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_CYAN, COL_BG);
        _lcd->drawString("CPU", COL_LEFT_X + 4, CPU_GPU_Y + 2);
        _lcd->setTextColor(COL_GREEN, COL_BG);
        _lcd->drawString("GPU", COL_RIGHT_X + 4, CPU_GPU_Y + 2);

        _lcd->setFont(&fonts::Font0);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawString(data.cpu_name, COL_LEFT_X + 40, CPU_GPU_Y + 6);
        _lcd->drawString(data.gpu_name, COL_RIGHT_X + 40, CPU_GPU_Y + 6);

        _first_draw = false;
    }

    // ========== CPU Section (left column) ==========
    int lx = COL_LEFT_X;
    int ly = CPU_GPU_Y + 20;

    // Load bar + percentage
    uint16_t cpuCol = (data.cpu_load < 70) ? COL_GREEN : (data.cpu_load < 90) ? COL_YELLOW : COL_RED;
    drawBar(lx + 4, ly, COL_LEFT_W - 55, BAR_H, data.cpu_load, cpuCol);
    snprintf(buf, sizeof(buf), "%3.0f%%", data.cpu_load);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, lx + COL_LEFT_W - 45, ly);

    // Temp + Clock + Power (one line)
    ly += BAR_H + 4;
    snprintf(buf, sizeof(buf), "%.0fC ", data.cpu_temp);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(tempColor(data.cpu_temp), COL_BG);
    _lcd->drawString(buf, lx + 4, ly);

    snprintf(buf, sizeof(buf), "%.0fMHz ", data.cpu_clock);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, lx + 55, ly);

    snprintf(buf, sizeof(buf), "%.0fW  ", data.cpu_power);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString(buf, lx + 148, ly);

    // Graph
    ly += 20;
    _cpu_history[_history_idx] = data.cpu_load;
    drawGraph(lx + 4, ly, GRAPH_W, GRAPH_H, _cpu_history, HISTORY_LEN, COL_CYAN);

    // ========== GPU Section (right column) ==========
    int rx = COL_RIGHT_X;
    int ry = CPU_GPU_Y + 20;

    // Load bar + percentage
    uint16_t gpuCol = (data.gpu_load < 70) ? COL_GREEN : (data.gpu_load < 90) ? COL_YELLOW : COL_RED;
    drawBar(rx + 4, ry, COL_RIGHT_W - 55, BAR_H, data.gpu_load, gpuCol);
    snprintf(buf, sizeof(buf), "%3.0f%%", data.gpu_load);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, rx + COL_RIGHT_W - 45, ry);

    // Temp + VRAM (one line)
    ry += BAR_H + 4;
    snprintf(buf, sizeof(buf), "%.0fC ", data.gpu_temp);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(tempColor(data.gpu_temp), COL_BG);
    _lcd->drawString(buf, rx + 4, ry);

    snprintf(buf, sizeof(buf), "%.0f/%.0fMB ", data.gpu_vram_used, data.gpu_vram_total);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, rx + 55, ry);

    // Graph
    ry += 20;
    _gpu_history[_history_idx] = data.gpu_load;
    drawGraph(rx + 4, ry, GRAPH_W, GRAPH_H, _gpu_history, HISTORY_LEN, COL_GREEN);

    // ========== RAM Section ==========
    int ry2 = RAM_Y + 5;
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_YELLOW, COL_BG);
    _lcd->drawString("RAM", 6, ry2);

    uint16_t ramCol = (data.ram_percent < 70) ? COL_GREEN : (data.ram_percent < 90) ? COL_YELLOW : COL_RED;
    drawBar(44, ry2, 240, BAR_H, data.ram_percent, ramCol);

    snprintf(buf, sizeof(buf), "%.0f%%  %.1f/%.0fGB  ", data.ram_percent, data.ram_used_gb, data.ram_total_gb);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 290, ry2);

    // ========== Storage Section ==========
    int sy = STORAGE_Y + 5;
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_BLUE, COL_BG);
    _lcd->drawString("DSK", 6, sy);

    float sPct = 0;
    if (data.storage_total_tb > 0)
        sPct = (data.storage_used_tb / data.storage_total_tb) * 100.0f;
    uint16_t sCol = (sPct < 70) ? COL_GREEN : (sPct < 90) ? COL_YELLOW : COL_RED;
    drawBar(44, sy, 240, BAR_H, sPct, sCol);

    snprintf(buf, sizeof(buf), "%.0f%% %.0f/%.0fTB ", sPct, data.storage_used_tb, data.storage_total_tb);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 290, sy);

    // ========== Bottom Section: Fans + Disk Temps ==========
    int by = BOTTOM_Y + 4;
    _lcd->setFont(&fonts::Font0);

    // Fans (left side)
    for (int i = 0; i < 2; i++) {
        int fx = 6 + i * 100;
        if (data.fan[i] >= 0) {
            snprintf(buf, sizeof(buf), "FAN%d:%dRPM ", i + 1, data.fan[i]);
            _lcd->setTextColor(COL_TEXT, COL_BG);
        } else {
            snprintf(buf, sizeof(buf), "FAN%d:---   ", i + 1);
            _lcd->setTextColor(COL_DIVIDER, COL_BG);
        }
        _lcd->drawString(buf, fx, by);
    }

    // Disk temps (right side of fan line + second line)
    int dtx = 210;  // start x for disk temps
    int dty = by;
    int col_w = 68; // width per disk temp entry
    int cols = (SCREEN_W - dtx) / col_w; // how many fit per row

    for (int i = 0; i < data.disk_count && i < 8; i++) {
        int cx = dtx + (i % cols) * col_w;
        int cy = dty + (i / cols) * 12;

        if (data.disk_temp[i] >= 0) {
            uint16_t tc = tempColor((float)data.disk_temp[i]);
            snprintf(buf, sizeof(buf), "%s:%dC", data.disk_name[i], data.disk_temp[i]);
            _lcd->setTextColor(tc, COL_BG);
        } else {
            snprintf(buf, sizeof(buf), "%s:--", data.disk_name[i]);
            _lcd->setTextColor(COL_DIVIDER, COL_BG);
        }
        _lcd->drawString(buf, cx, cy);
    }

    // Connection dot (top right corner)
    _lcd->fillCircle(SCREEN_W - 8, 6, 4, data.connected ? COL_GREEN : COL_RED);

    // Update history
    _history_idx = (_history_idx + 1) % HISTORY_LEN;
}
