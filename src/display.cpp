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
    lcd.setRotation(1);  // Landscape
    lcd.setBrightness(200);
    lcd.fillScreen(COL_BG);

    printf("Display: ST7796S 480x320 initialized (direct draw, no flicker)\n");
    printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void PCMonitorDisplay::drawHeader() {
    _lcd->fillRect(0, 0, SCREEN_W, HEADER_H, COL_HEADER);
    _lcd->setTextColor(COL_CYAN, COL_HEADER);
    _lcd->setFont(&fonts::Font4);
    _lcd->drawString("PC HARDWARE MONITOR", 10, 4);
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

    // Draw filled portion
    if (filled > 0) {
        _lcd->fillRect(x, y, filled, h, color);
    }
    // Draw empty portion (overwrites old bar without clearing first)
    if (filled < w) {
        _lcd->fillRect(x + filled, y, w - filled, h, COL_BAR_BG);
    }
    _lcd->drawRect(x, y, w, h, COL_DIVIDER);
}

void PCMonitorDisplay::drawGraph(int x, int y, int w, int h, float *history, int len, uint16_t color) {
    // Clear graph area
    _lcd->fillRect(x + 1, y + 1, w - 2, h - 2, COL_BAR_BG);
    _lcd->drawRect(x, y, w, h, COL_DIVIDER);

    // Grid lines at 25%, 50%, 75%
    for (int g = 1; g <= 3; g++) {
        int gy = y + h - (h * g / 4);
        for (int gx = x + 2; gx < x + w - 2; gx += 4) {
            _lcd->drawPixel(gx, gy, COL_DIVIDER);
        }
    }

    // Plot values
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

void PCMonitorDisplay::drawTemp(int x, int y, float temp) {
    char buf[16];
    uint16_t col = tempColor(temp);
    snprintf(buf, sizeof(buf), "%.0f C  ", temp);  // trailing spaces to clear old digits
    _lcd->setTextColor(col, COL_BG);
    _lcd->setFont(&fonts::Font4);
    _lcd->drawString(buf, x, y);
}

void PCMonitorDisplay::drawFanSection(const HWData &data) {
    int y = FAN_Y + 6;

    _lcd->setFont(&fonts::Font2);

    char buf[32];
    for (int i = 0; i < 4; i++) {
        int fx = (i % 2 == 0) ? COL_LEFT_X + 10 : COL_RIGHT_X + 10;
        int fy = y + (i / 2) * 20;

        if (data.fan[i] >= 0) {
            snprintf(buf, sizeof(buf), "FAN%d: %d RPM   ", i + 1, data.fan[i]);
            _lcd->setTextColor(COL_TEXT, COL_BG);
        } else {
            snprintf(buf, sizeof(buf), "FAN%d: ---      ", i + 1);
            _lcd->setTextColor(COL_DIVIDER, COL_BG);
        }
        _lcd->drawString(buf, fx, fy);
    }
}

void PCMonitorDisplay::drawStorageSection(const HWData &data) {
    int y = STORAGE_Y;
    y += 6;

    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_BLUE, COL_BG);
    _lcd->drawString("DISK", COL_LEFT_X + 4, y + 2);

    float pct = 0;
    if (data.storage_total_tb > 0) {
        pct = (data.storage_used_tb / data.storage_total_tb) * 100.0f;
    }

    uint16_t col = (pct < 70) ? COL_GREEN : (pct < 90) ? COL_YELLOW : COL_RED;
    drawBar(60, y, 280, BAR_H, pct, col);

    char buf[64];
    snprintf(buf, sizeof(buf), "%.0f%%  %.1f / %.1f TB ", pct, data.storage_used_tb, data.storage_total_tb);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 350, y + 2);
}

void PCMonitorDisplay::showWaiting() {
    _lcd->fillScreen(COL_BG);
    drawHeader();

    _lcd->setFont(&fonts::Font4);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawCenterString("Waiting for PC connection...", SCREEN_W / 2, 120);
    _lcd->setFont(&fonts::Font2);
    _lcd->drawCenterString("Start LibreHardwareMonitor + pc_monitor.py", SCREEN_W / 2, 160);
    _lcd->drawCenterString("on your Windows PC", SCREEN_W / 2, 180);
}

void PCMonitorDisplay::update(const HWData &data) {
    char buf[64];

    // On first draw: clear screen and draw all static elements
    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawHeader();

        // Draw static dividers
        _lcd->drawFastVLine(COL_RIGHT_X - 2, CPU_GPU_Y, CPU_GPU_H, COL_DIVIDER);
        _lcd->drawFastHLine(4, RAM_Y, SCREEN_W - 8, COL_DIVIDER);
        _lcd->drawFastHLine(4, STORAGE_Y, SCREEN_W - 8, COL_DIVIDER);
        _lcd->drawFastHLine(4, FAN_Y, SCREEN_W - 8, COL_DIVIDER);

        // Static labels
        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_CYAN, COL_BG);
        _lcd->drawString("CPU", COL_LEFT_X + 4, CPU_GPU_Y + 2);

        _lcd->setTextColor(COL_GREEN, COL_BG);
        _lcd->drawString("GPU", COL_RIGHT_X + 4, CPU_GPU_Y + 2);

        // CPU/GPU names (only once)
        _lcd->setFont(&fonts::Font0);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawString(data.cpu_name, COL_LEFT_X + 40, CPU_GPU_Y + 6);
        _lcd->drawString(data.gpu_name, COL_RIGHT_X + 40, CPU_GPU_Y + 6);

        // Temp labels
        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawString("Temp:", COL_LEFT_X + 4, CPU_GPU_Y + 22 + BAR_H + 6);
        _lcd->drawString("Temp:", COL_RIGHT_X + 4, CPU_GPU_Y + 22 + BAR_H + 6);

        _first_draw = false;
    }

    // === Connection status dot ===
    uint16_t dotCol = data.connected ? COL_GREEN : COL_RED;
    _lcd->fillCircle(SCREEN_W - 14, HEADER_H / 2, 5, dotCol);

    // === CPU Section (left) — dynamic values only ===
    int lx = COL_LEFT_X;
    int ly = CPU_GPU_Y + 22;

    // CPU load bar + percentage
    uint16_t cpuCol = (data.cpu_load < 70) ? COL_GREEN : (data.cpu_load < 90) ? COL_YELLOW : COL_RED;
    drawBar(lx + 4, ly, COL_LEFT_W - 55, BAR_H, data.cpu_load, cpuCol);
    snprintf(buf, sizeof(buf), "%3.0f%% ", data.cpu_load);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, lx + COL_LEFT_W - 45, ly);

    // CPU temp
    ly += BAR_H + 6;
    drawTemp(lx + 60, ly, data.cpu_temp);

    // CPU graph
    ly += 24;
    _cpu_history[_history_idx] = data.cpu_load;
    drawGraph(lx + 4, ly, GRAPH_W, GRAPH_H, _cpu_history, HISTORY_LEN, COL_CYAN);

    // === GPU Section (right) — dynamic values only ===
    int rx = COL_RIGHT_X;
    int ry = CPU_GPU_Y + 22;

    // GPU load bar + percentage
    uint16_t gpuCol = (data.gpu_load < 70) ? COL_GREEN : (data.gpu_load < 90) ? COL_YELLOW : COL_RED;
    drawBar(rx + 4, ry, COL_RIGHT_W - 55, BAR_H, data.gpu_load, gpuCol);
    snprintf(buf, sizeof(buf), "%3.0f%% ", data.gpu_load);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, rx + COL_RIGHT_W - 45, ry);

    // GPU temp
    ry += BAR_H + 6;
    drawTemp(rx + 60, ry, data.gpu_temp);

    // GPU graph
    ry += 24;
    _gpu_history[_history_idx] = data.gpu_load;
    drawGraph(rx + 4, ry, GRAPH_W, GRAPH_H, _gpu_history, HISTORY_LEN, COL_GREEN);

    // === RAM Section ===
    int ry2 = RAM_Y + 6;
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_YELLOW, COL_BG);
    _lcd->drawString("RAM", COL_LEFT_X + 4, ry2 + 2);

    uint16_t ramCol = (data.ram_percent < 70) ? COL_GREEN : (data.ram_percent < 90) ? COL_YELLOW : COL_RED;
    drawBar(60, ry2, 280, BAR_H, data.ram_percent, ramCol);

    snprintf(buf, sizeof(buf), "%3.0f%%  %.1f / %.0f GB ", data.ram_percent, data.ram_used_gb, data.ram_total_gb);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 350, ry2 + 2);

    // === Storage Section ===
    drawStorageSection(data);

    // === Fan Section ===
    drawFanSection(data);

    // Update history index
    _history_idx = (_history_idx + 1) % HISTORY_LEN;
}
