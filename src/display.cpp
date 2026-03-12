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
        // SPI bus
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

        // Panel
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

        // Backlight
        auto lcfg = _light.config();
        lcfg.pin_bl = TFT_BL;
        lcfg.invert = false;
        lcfg.freq = 44100;
        lcfg.pwm_channel = 7;
        _light.config(lcfg);
        _panel.setLight(&_light);

        // Touch
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
static LGFX_Sprite sprite(&lcd);

PCMonitorDisplay display;

void PCMonitorDisplay::init() {
    _lcd = &lcd;
    _sprite = &sprite;
    _history_idx = 0;
    _first_draw = true;
    memset(_cpu_history, 0, sizeof(_cpu_history));
    memset(_gpu_history, 0, sizeof(_gpu_history));

    lcd.init();
    lcd.setRotation(1);  // Landscape
    lcd.setBrightness(200);
    lcd.fillScreen(COL_BG);

    // Create full-screen sprite for double buffering
    sprite.setColorDepth(16);
    sprite.createSprite(SCREEN_W, SCREEN_H);
    sprite.fillSprite(COL_BG);

    printf("Display: ST7796S 480x320 initialized\n");
}

void PCMonitorDisplay::drawHeader() {
    _sprite->fillRect(0, 0, SCREEN_W, HEADER_H, COL_HEADER);
    _sprite->setTextColor(COL_CYAN, COL_HEADER);
    _sprite->setFont(&fonts::Font4);
    _sprite->drawString("PC HARDWARE MONITOR", 10, 4);
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

    _sprite->fillRect(x, y, w, h, COL_BAR_BG);
    if (filled > 0) {
        _sprite->fillRect(x, y, filled, h, color);
    }
    // Border
    _sprite->drawRect(x, y, w, h, COL_DIVIDER);
}

void PCMonitorDisplay::drawGraph(int x, int y, int w, int h, float *history, int len, uint16_t color) {
    // Background
    _sprite->fillRect(x, y, w, h, COL_BAR_BG);
    _sprite->drawRect(x, y, w, h, COL_DIVIDER);

    // Grid lines at 25%, 50%, 75%
    for (int g = 1; g <= 3; g++) {
        int gy = y + h - (h * g / 4);
        for (int gx = x + 2; gx < x + w - 2; gx += 4) {
            _sprite->drawPixel(gx, gy, COL_DIVIDER);
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

        _sprite->drawLine(x0, y0, x1, y1, color);
    }
}

void PCMonitorDisplay::drawTemp(int x, int y, float temp) {
    char buf[16];
    uint16_t col = tempColor(temp);
    snprintf(buf, sizeof(buf), "%.0f%cC", temp, 0xB0);  // degree symbol
    _sprite->setTextColor(col, COL_BG);
    _sprite->setFont(&fonts::Font4);
    _sprite->drawString(buf, x, y);
}

void PCMonitorDisplay::drawFanSection(const HWData &data) {
    int y = FAN_Y;
    _sprite->fillRect(0, y, SCREEN_W, FAN_H, COL_BG);
    _sprite->drawFastHLine(4, y, SCREEN_W - 8, COL_DIVIDER);
    y += 6;

    _sprite->setFont(&fonts::Font2);
    _sprite->setTextColor(COL_LABEL, COL_BG);

    char buf[32];
    for (int i = 0; i < 4; i++) {
        int fx = (i % 2 == 0) ? COL_LEFT_X + 10 : COL_RIGHT_X + 10;
        int fy = y + (i / 2) * 20;

        if (data.fan[i] >= 0) {
            snprintf(buf, sizeof(buf), "FAN%d: %d RPM", i + 1, data.fan[i]);
            _sprite->setTextColor(COL_TEXT, COL_BG);
        } else {
            snprintf(buf, sizeof(buf), "FAN%d: ---", i + 1);
            _sprite->setTextColor(COL_DIVIDER, COL_BG);
        }
        _sprite->drawString(buf, fx, fy);
    }
}

void PCMonitorDisplay::showWaiting() {
    sprite.fillSprite(COL_BG);
    drawHeader();

    _sprite->setFont(&fonts::Font4);
    _sprite->setTextColor(COL_LABEL, COL_BG);
    _sprite->drawCenterString("Waiting for PC connection...", SCREEN_W / 2, 120);
    _sprite->setFont(&fonts::Font2);
    _sprite->drawCenterString("Start LibreHardwareMonitor + pc_monitor.py", SCREEN_W / 2, 160);
    _sprite->drawCenterString("on your Windows PC", SCREEN_W / 2, 180);

    sprite.pushSprite(0, 0);
}

void PCMonitorDisplay::update(const HWData &data) {
    char buf[64];

    sprite.fillSprite(COL_BG);
    drawHeader();

    // === Connection status ===
    if (data.connected) {
        _sprite->fillCircle(SCREEN_W - 14, HEADER_H / 2, 5, COL_GREEN);
    } else {
        _sprite->fillCircle(SCREEN_W - 14, HEADER_H / 2, 5, COL_RED);
    }

    // === CPU Section (left) ===
    int lx = COL_LEFT_X;
    int ly = CPU_GPU_Y;

    _sprite->setFont(&fonts::Font2);
    _sprite->setTextColor(COL_CYAN, COL_BG);
    _sprite->drawString("CPU", lx + 4, ly + 2);

    _sprite->setFont(&fonts::Font0);
    _sprite->setTextColor(COL_LABEL, COL_BG);
    _sprite->drawString(data.cpu_name, lx + 40, ly + 6);

    // CPU load bar
    ly += 22;
    uint16_t cpuCol = (data.cpu_load < 70) ? COL_GREEN : (data.cpu_load < 90) ? COL_YELLOW : COL_RED;
    drawBar(lx + 4, ly, COL_LEFT_W - 55, BAR_H, data.cpu_load, cpuCol);
    snprintf(buf, sizeof(buf), "%.0f%%", data.cpu_load);
    _sprite->setFont(&fonts::Font2);
    _sprite->setTextColor(COL_TEXT, COL_BG);
    _sprite->drawString(buf, lx + COL_LEFT_W - 45, ly);

    // CPU temp
    ly += BAR_H + 6;
    _sprite->setTextColor(COL_LABEL, COL_BG);
    _sprite->drawString("Temp:", lx + 4, ly);
    drawTemp(lx + 60, ly, data.cpu_temp);

    // CPU graph
    ly += 24;
    _cpu_history[_history_idx] = data.cpu_load;
    drawGraph(lx + 4, ly, GRAPH_W, GRAPH_H, _cpu_history, HISTORY_LEN, COL_CYAN);

    // === GPU Section (right) ===
    int rx = COL_RIGHT_X;
    int ry = CPU_GPU_Y;

    _sprite->setFont(&fonts::Font2);
    _sprite->setTextColor(COL_GREEN, COL_BG);
    _sprite->drawString("GPU", rx + 4, ry + 2);

    _sprite->setFont(&fonts::Font0);
    _sprite->setTextColor(COL_LABEL, COL_BG);
    _sprite->drawString(data.gpu_name, rx + 40, ry + 6);

    // GPU load bar
    ry += 22;
    uint16_t gpuCol = (data.gpu_load < 70) ? COL_GREEN : (data.gpu_load < 90) ? COL_YELLOW : COL_RED;
    drawBar(rx + 4, ry, COL_RIGHT_W - 55, BAR_H, data.gpu_load, gpuCol);
    snprintf(buf, sizeof(buf), "%.0f%%", data.gpu_load);
    _sprite->setFont(&fonts::Font2);
    _sprite->setTextColor(COL_TEXT, COL_BG);
    _sprite->drawString(buf, rx + COL_RIGHT_W - 45, ry);

    // GPU temp
    ry += BAR_H + 6;
    _sprite->setTextColor(COL_LABEL, COL_BG);
    _sprite->drawString("Temp:", rx + 4, ry);
    drawTemp(rx + 60, ry, data.gpu_temp);

    // GPU graph
    ry += 24;
    _gpu_history[_history_idx] = data.gpu_load;
    drawGraph(rx + 4, ry, GRAPH_W, GRAPH_H, _gpu_history, HISTORY_LEN, COL_GREEN);

    // Divider between CPU and GPU
    _sprite->drawFastVLine(COL_RIGHT_X - 2, CPU_GPU_Y, CPU_GPU_H, COL_DIVIDER);

    // === RAM Section ===
    int ry2 = RAM_Y;
    _sprite->drawFastHLine(4, ry2, SCREEN_W - 8, COL_DIVIDER);
    ry2 += 6;

    _sprite->setFont(&fonts::Font2);
    _sprite->setTextColor(COL_YELLOW, COL_BG);
    _sprite->drawString("RAM", COL_LEFT_X + 4, ry2 + 2);

    uint16_t ramCol = (data.ram_percent < 70) ? COL_GREEN : (data.ram_percent < 90) ? COL_YELLOW : COL_RED;
    drawBar(60, ry2, 280, BAR_H, data.ram_percent, ramCol);

    snprintf(buf, sizeof(buf), "%.0f%%  %.1f / %.0f GB", data.ram_percent, data.ram_used_gb, data.ram_total_gb);
    _sprite->setTextColor(COL_TEXT, COL_BG);
    _sprite->drawString(buf, 350, ry2 + 2);

    // === Fan Section ===
    drawFanSection(data);

    // Update history index
    _history_idx = (_history_idx + 1) % HISTORY_LEN;

    // Push to screen
    sprite.pushSprite(0, 0);
}
