#include "display.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <Preferences.h>

static Preferences nvs;

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

// Helper: format network speed as readable string
static void formatSpeed(float kbps, char *buf, int maxLen) {
    float mbps = kbps / 1024.0f;
    if (mbps >= 1024.0f) {
        snprintf(buf, maxLen, "%.2f GB/s", mbps / 1024.0f);
    } else if (mbps >= 1.0f) {
        snprintf(buf, maxLen, "%.1f MB/s", mbps);
    } else if (kbps >= 1.0f) {
        snprintf(buf, maxLen, "%.0f KB/s", kbps);
    } else {
        snprintf(buf, maxLen, "0 KB/s");
    }
}

void PCMonitorDisplay::init() {
    _lcd = &lcd;
    _sprite = nullptr;
    _history_idx = 0;
    _first_draw = true;
    _screen = SCREEN_MAIN;
    _lastTouchTime = 0;
    _lastTimestamp = 0;
    _tzOffset = 3600;  // Default CET
    _lastTimeSyncMillis = 0;
    _disconnectMillis = 0;
    _dotAnimState = 0;
    _lastDotAnim = 0;

    // Restore last known time from NVS (survives reboot)
    nvs.begin("hwmon", false);
    unsigned long savedTs = nvs.getULong("ts", 0);
    int savedTzo = nvs.getInt("tzo", 3600);
    if (savedTs > 1700000000UL) {  // Sanity check: after 2023
        _lastTimestamp = savedTs;
        _tzOffset = savedTzo;
        _lastTimeSyncMillis = millis();
        _timeValid = true;
        printf("Restored time from NVS: ts=%lu tzo=%d\n", savedTs, savedTzo);
    } else {
        _timeValid = false;
    }

    memset(_cpu_history, 0, sizeof(_cpu_history));
    memset(_gpu_history, 0, sizeof(_gpu_history));
    memset(_ram_history, 0, sizeof(_ram_history));
    memset(_net_dl_history, 0, sizeof(_net_dl_history));
    memset(_net_ul_history, 0, sizeof(_net_ul_history));

    lcd.init();
    lcd.setRotation(1);
    lcd.setBrightness(200);
    lcd.fillScreen(COL_BG);

    printf("Display initialized (480x320, touch enabled)\n");
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

void PCMonitorDisplay::drawBackButton() {
    _lcd->fillRoundRect(4, 4, BACK_BTN_W, BACK_BTN_H, 4, COL_DIVIDER);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_DIVIDER);
    _lcd->drawString("<Back", 10, 8);
}

void PCMonitorDisplay::drawNextButton() {
    int x = SCREEN_W - NEXT_BTN_W - 4;
    _lcd->fillRoundRect(x, 4, NEXT_BTN_W, NEXT_BTN_H, 4, COL_DIVIDER);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_DIVIDER);
    _lcd->drawRightString("Next>", SCREEN_W - 10, 8);
}

ScreenState PCMonitorDisplay::nextDetailScreen(ScreenState current) {
    switch (current) {
        case SCREEN_CPU_DETAIL:  return SCREEN_GPU_DETAIL;
        case SCREEN_GPU_DETAIL:  return SCREEN_RAM_DETAIL;
        case SCREEN_RAM_DETAIL:  return SCREEN_DISK_DETAIL;
        case SCREEN_DISK_DETAIL: return SCREEN_FAN_DETAIL;
        case SCREEN_FAN_DETAIL:  return SCREEN_NET_DETAIL;
        case SCREEN_NET_DETAIL:  return SCREEN_CPU_DETAIL;  // wrap around
        default:                 return SCREEN_MAIN;
    }
}

ScreenState PCMonitorDisplay::nextAdvDetailScreen(ScreenState current) {
    switch (current) {
        case SCREEN_ADV_MOBO: return SCREEN_ADV_CPU;
        case SCREEN_ADV_CPU:  return SCREEN_ADV_GPU;
        case SCREEN_ADV_GPU:  return SCREEN_ADV_RAM;
        case SCREEN_ADV_RAM:  return SCREEN_ADV_DISK;
        case SCREEN_ADV_DISK: return SCREEN_ADV_MOBO;  // wrap around
        default:              return SCREEN_ADV_MAIN;
    }
}

void PCMonitorDisplay::drawAdvButton() {
    // "..." button bottom-right of main screen
    int bw = 36, bh = 20;
    int bx = SCREEN_W - bw - 6;
    int by = SCREEN_H - bh - 4;
    _lcd->fillRoundRect(bx, by, bw, bh, 4, COL_PURPLE);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_PURPLE);
    _lcd->drawCenterString("...", bx + bw / 2, by + 3);
}

void PCMonitorDisplay::drawMainButton() {
    // "MAIN" button top-left of advanced main screen
    _lcd->fillRoundRect(4, 4, 50, BACK_BTN_H, 4, COL_DIVIDER);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_DIVIDER);
    _lcd->drawString("MAIN", 8, 8);
}

void PCMonitorDisplay::drawCurrentScreen(const HWData &data) {
    switch (_screen) {
        case SCREEN_CPU_DETAIL:  drawCpuDetail(data); break;
        case SCREEN_GPU_DETAIL:  drawGpuDetail(data); break;
        case SCREEN_RAM_DETAIL:  drawRamDetail(data); break;
        case SCREEN_DISK_DETAIL: drawDiskDetail(data); break;
        case SCREEN_FAN_DETAIL:  drawFanDetail(data); break;
        case SCREEN_NET_DETAIL:  drawNetDetail(data); break;
        case SCREEN_ADV_MAIN:    drawAdvMainScreen(data); break;
        case SCREEN_ADV_MOBO:    drawAdvMoboDetail(data); break;
        case SCREEN_ADV_CPU:     drawAdvCpuDetail(data); break;
        case SCREEN_ADV_GPU:     drawAdvGpuDetail(data); break;
        case SCREEN_ADV_RAM:     drawAdvRamDetail(data); break;
        case SCREEN_ADV_DISK:    drawAdvDiskDetail(data); break;
        default: break;
    }
}

void PCMonitorDisplay::showStandby() {
    _disconnectMillis = millis();
    _screen = SCREEN_STANDBY;
    _first_draw = true;
    _dotAnimState = 0;
    _lastDotAnim = 0;
    _lcd->fillScreen(COL_STANDBY_BG);
    _lcd->setBrightness(STANDBY_BRIGHTNESS);
    drawStandbyScreen();
}

void PCMonitorDisplay::updateStandby() {
    // Update clock every 60 seconds, dot animation every 800ms
    unsigned long now = millis();

    bool needClockUpdate = (now - _lastDotAnim >= 800);
    if (!needClockUpdate) return;

    drawStandbyScreen();
}

// German day/month names
static const char* DAYS_DE[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
static const char* MONTHS_DE[] = {"", "Januar", "Februar", "Maerz", "April", "Mai", "Juni",
                                   "Juli", "August", "September", "Oktober", "November", "Dezember"};

// Helper: convert Unix timestamp to broken-down time components
static void unixToTime(unsigned long ts, int &year, int &month, int &day,
                        int &hour, int &minute, int &second, int &wday) {
    // Simple Unix timestamp to date conversion
    unsigned long t = ts;
    second = t % 60; t /= 60;
    minute = t % 60; t /= 60;
    hour = t % 24;   t /= 24;

    // Days since epoch (1970-01-01 was Thursday = wday 4)
    wday = (t + 4) % 7;  // 0=Sunday

    // Year calculation
    year = 1970;
    while (true) {
        int daysInYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
        if (t < (unsigned long)daysInYear) break;
        t -= daysInYear;
        year++;
    }

    // Month calculation
    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) daysInMonth[1] = 29;

    month = 0;
    while (month < 12 && t >= (unsigned long)daysInMonth[month]) {
        t -= daysInMonth[month];
        month++;
    }
    month++;  // 1-based
    day = t + 1;
}

void PCMonitorDisplay::drawStandbyScreen() {
    char buf[64];
    unsigned long now = millis();

    if (_first_draw) {
        _lcd->fillScreen(COL_STANDBY_BG);
        _first_draw = false;
    }

    // Calculate current local time
    if (_timeValid) {
        unsigned long elapsed = (now - _lastTimeSyncMillis) / 1000;
        unsigned long localTs = _lastTimestamp + _tzOffset + elapsed;

        int year, month, day, hour, minute, second, wday;
        unixToTime(localTs, year, month, day, hour, minute, second, wday);

        // === TIME (large, centered, dim white) ===
        snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);

        _lcd->setFont(&fonts::Font7);  // Large 7-segment style
        _lcd->setTextColor(COL_STANDBY_TIME, COL_STANDBY_BG);
        _lcd->drawCenterString(buf, SCREEN_W / 2, 90);

        // === DATE (German format: "Do, 12. Maerz 2026") ===
        const char* dayName = (wday >= 0 && wday < 7) ? DAYS_DE[wday] : "??";
        const char* monthName = (month >= 1 && month <= 12) ? MONTHS_DE[month] : "???";
        snprintf(buf, sizeof(buf), "%s, %d. %s %d", dayName, day, monthName, year);

        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_STANDBY_DATE, COL_STANDBY_BG);
        _lcd->drawCenterString(buf, SCREEN_W / 2, 175);

        // === DISCONNECT COUNTER (bottom right) ===
        unsigned long discSec = (now - _disconnectMillis) / 1000;
        unsigned long discMin = discSec / 60;

        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_STANDBY_DOT, COL_STANDBY_BG);
        if (discMin > 0) {
            snprintf(buf, sizeof(buf), "%lu min ", discMin);
        } else {
            snprintf(buf, sizeof(buf), "%lu s  ", discSec);
        }
        _lcd->drawRightString(buf, SCREEN_W - 16, SCREEN_H - 22);
    } else {
        // No time data yet — minimal display
        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_STANDBY_DATE, COL_STANDBY_BG);
        _lcd->drawCenterString("--:--", SCREEN_W / 2, 110);
    }

    // === THREE DOTS ANIMATION (bottom left, subtle) ===
    _dotAnimState = (_dotAnimState + 1) % 4;
    _lastDotAnim = now;

    int dotX = 20;
    int dotY = SCREEN_H - 20;
    int dotR = 3;
    int dotSpacing = 14;

    for (int i = 0; i < 3; i++) {
        uint16_t col = (i < _dotAnimState) ? COL_STANDBY_DOT : COL_STANDBY_BG;
        _lcd->fillCircle(dotX + i * dotSpacing, dotY, dotR, col);
    }
    // When all 3 are lit, next frame clears all (state 0)
}

void PCMonitorDisplay::handleTouch(const HWData &data) {
    lgfx::touch_point_t tp;
    if (!_lcd->getTouch(&tp)) return;

    // Debounce
    if (millis() - _lastTouchTime < TOUCH_DEBOUNCE_MS) return;
    _lastTouchTime = millis();

    if (_screen == SCREEN_MAIN) {
        // Check "..." button (bottom right)
        if (tp.x > SCREEN_W - 50 && tp.y > SCREEN_H - 30) {
            _screen = SCREEN_ADV_MAIN;
            _first_draw = true;
            _lcd->fillScreen(COL_BG);
            drawAdvMainScreen(data);
            return;
        }

        // Determine which zone was tapped
        ScreenState newScreen = SCREEN_MAIN;
        if (tp.y >= CPU_GPU_Y && tp.y < RAM_Y) {
            if (tp.x < COL_RIGHT_X - 2) {
                newScreen = SCREEN_CPU_DETAIL;
            } else {
                newScreen = SCREEN_GPU_DETAIL;
            }
        } else if (tp.y >= RAM_Y && tp.y < STORAGE_Y) {
            newScreen = SCREEN_RAM_DETAIL;
        } else if (tp.y >= STORAGE_Y && tp.y < NET_Y) {
            newScreen = SCREEN_DISK_DETAIL;
        } else if (tp.y >= NET_Y && tp.y < BOTTOM_Y) {
            newScreen = SCREEN_NET_DETAIL;
        } else if (tp.y >= BOTTOM_Y) {
            // Bottom section: left half = fans, right half = disk temps
            if (tp.x < 210) {
                newScreen = SCREEN_FAN_DETAIL;
            } else {
                newScreen = SCREEN_DISK_DETAIL;
            }
        }

        if (newScreen != SCREEN_MAIN) {
            _screen = newScreen;
            _first_draw = true;
            _lcd->fillScreen(COL_BG);
            drawCurrentScreen(data);
        }
    } else if (_screen == SCREEN_ADV_MAIN) {
        // "MAIN" button (top left) → go back to main screen
        if (tp.x < 60 && tp.y < BACK_BTN_H + 10) {
            _screen = SCREEN_MAIN;
            _first_draw = true;
            _lcd->fillScreen(COL_BG);
            return;
        }
        // Tap zones on Advanced main → go to detail
        ScreenState newScreen = SCREEN_ADV_MAIN;
        if (tp.y >= 34 && tp.y < 130) {
            if (tp.x < SCREEN_W / 2) {
                newScreen = SCREEN_ADV_MOBO;
            } else {
                newScreen = SCREEN_ADV_CPU;
            }
        } else if (tp.y >= 130 && tp.y < 180) {
            newScreen = SCREEN_ADV_RAM;
        } else if (tp.y >= 180 && tp.y < 240) {
            newScreen = SCREEN_ADV_DISK;
        } else if (tp.y >= 240) {
            newScreen = SCREEN_ADV_GPU;
        }

        if (newScreen != SCREEN_ADV_MAIN) {
            _screen = newScreen;
            _first_draw = true;
            _lcd->fillScreen(COL_BG);
            drawCurrentScreen(data);
        }
    } else if (_screen >= SCREEN_ADV_MOBO && _screen <= SCREEN_ADV_DISK) {
        // Advanced detail screens: Back → ADV_MAIN, Next → cycle
        if (tp.x < BACK_BTN_W + 10 && tp.y < BACK_BTN_H + 10) {
            _screen = SCREEN_ADV_MAIN;
            _first_draw = true;
            _lcd->fillScreen(COL_BG);
        }
        else if (tp.x > SCREEN_W - NEXT_BTN_W - 10 && tp.y < NEXT_BTN_H + 10) {
            _screen = nextAdvDetailScreen(_screen);
            _first_draw = true;
            _lcd->fillScreen(COL_BG);
            drawCurrentScreen(data);
        }
    } else {
        // Basic detail screens: check back button (top left)
        if (tp.x < BACK_BTN_W + 10 && tp.y < BACK_BTN_H + 10) {
            _screen = SCREEN_MAIN;
            _first_draw = true;
            _lcd->fillScreen(COL_BG);
        }
        // Check next button (top right)
        else if (tp.x > SCREEN_W - NEXT_BTN_W - 10 && tp.y < NEXT_BTN_H + 10) {
            _screen = nextDetailScreen(_screen);
            _first_draw = true;
            _lcd->fillScreen(COL_BG);
            drawCurrentScreen(data);
        }
    }
}

void PCMonitorDisplay::update(const HWData &data) {
    // Sync time from PC
    if (data.pc_timestamp > 0) {
        _lastTimestamp = data.pc_timestamp;
        _tzOffset = data.tz_offset;
        _lastTimeSyncMillis = millis();
        _timeValid = true;

        // Save to NVS every 30 seconds (avoid excessive flash writes)
        static unsigned long lastNvsSave = 0;
        if (millis() - lastNvsSave > 30000) {
            nvs.putULong("ts", _lastTimestamp);
            nvs.putInt("tzo", _tzOffset);
            lastNvsSave = millis();
        }
    }

    // Coming back from standby?
    if (_screen == SCREEN_STANDBY) {
        _screen = SCREEN_MAIN;
        _first_draw = true;
        _lcd->setBrightness(200);
        _lcd->fillScreen(COL_BG);
    }

    switch (_screen) {
        case SCREEN_MAIN:        drawMainScreen(data); break;
        case SCREEN_CPU_DETAIL:  drawCpuDetail(data); break;
        case SCREEN_GPU_DETAIL:  drawGpuDetail(data); break;
        case SCREEN_RAM_DETAIL:  drawRamDetail(data); break;
        case SCREEN_DISK_DETAIL: drawDiskDetail(data); break;
        case SCREEN_FAN_DETAIL:  drawFanDetail(data); break;
        case SCREEN_NET_DETAIL:  drawNetDetail(data); break;
        case SCREEN_ADV_MAIN:    drawAdvMainScreen(data); break;
        case SCREEN_ADV_MOBO:    drawAdvMoboDetail(data); break;
        case SCREEN_ADV_CPU:     drawAdvCpuDetail(data); break;
        case SCREEN_ADV_GPU:     drawAdvGpuDetail(data); break;
        case SCREEN_ADV_RAM:     drawAdvRamDetail(data); break;
        case SCREEN_ADV_DISK:    drawAdvDiskDetail(data); break;
        case SCREEN_STANDBY:     break;  // handled by updateStandby()
    }

    // Update history (always, regardless of screen)
    _cpu_history[_history_idx] = data.cpu_load;
    _gpu_history[_history_idx] = data.gpu_load;
    _ram_history[_history_idx] = data.ram_percent;
    // Normalize net speeds to 0-100 for graph (auto-scale: find max in history)
    float maxDl = 1.0f;
    float maxUl = 1.0f;
    for (int i = 0; i < HISTORY_LEN; i++) {
        if (_net_dl_history[i] > maxDl) maxDl = _net_dl_history[i];
        if (_net_ul_history[i] > maxUl) maxUl = _net_ul_history[i];
    }
    // Store raw KB/s values; graph will normalize when drawing
    _net_dl_history[_history_idx] = data.net_download;
    _net_ul_history[_history_idx] = data.net_upload;
    _history_idx = (_history_idx + 1) % HISTORY_LEN;
}


// ==================== MAIN SCREEN ====================

void PCMonitorDisplay::drawMainScreen(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        _lcd->drawFastVLine(COL_RIGHT_X - 2, CPU_GPU_Y, CPU_GPU_H, COL_DIVIDER);
        _lcd->drawFastHLine(4, RAM_Y, SCREEN_W - 8, COL_DIVIDER);
        _lcd->drawFastHLine(4, STORAGE_Y, SCREEN_W - 8, COL_DIVIDER);
        _lcd->drawFastHLine(4, NET_Y, SCREEN_W - 8, COL_DIVIDER);
        _lcd->drawFastHLine(4, BOTTOM_Y, SCREEN_W - 8, COL_DIVIDER);

        // CPU/GPU labels (static)
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

    uint16_t cpuCol = (data.cpu_load < 70) ? COL_GREEN : (data.cpu_load < 90) ? COL_YELLOW : COL_RED;
    drawBar(lx + 4, ly, COL_LEFT_W - 55, BAR_H, data.cpu_load, cpuCol);
    snprintf(buf, sizeof(buf), "%3.0f%%", data.cpu_load);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, lx + COL_LEFT_W - 45, ly);

    ly += BAR_H + 4;
    snprintf(buf, sizeof(buf), "%.0f C ", data.cpu_temp);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(tempColor(data.cpu_temp), COL_BG);
    _lcd->drawString(buf, lx + 4, ly);

    snprintf(buf, sizeof(buf), " %.0f MHz ", data.cpu_clock);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, lx + 55, ly);

    snprintf(buf, sizeof(buf), " %.0f W  ", data.cpu_power);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString(buf, lx + 150, ly);

    ly += 20;
    drawGraph(lx + 4, ly, GRAPH_W, GRAPH_H, _cpu_history, HISTORY_LEN, COL_CYAN);

    // ========== GPU Section (right column) ==========
    int rx = COL_RIGHT_X;
    int ry = CPU_GPU_Y + 20;

    uint16_t gpuCol = (data.gpu_load < 70) ? COL_GREEN : (data.gpu_load < 90) ? COL_YELLOW : COL_RED;
    drawBar(rx + 4, ry, COL_RIGHT_W - 55, BAR_H, data.gpu_load, gpuCol);
    snprintf(buf, sizeof(buf), "%3.0f%%", data.gpu_load);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, rx + COL_RIGHT_W - 45, ry);

    ry += BAR_H + 4;
    snprintf(buf, sizeof(buf), "%.0f C ", data.gpu_temp);
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(tempColor(data.gpu_temp), COL_BG);
    _lcd->drawString(buf, rx + 4, ry);

    snprintf(buf, sizeof(buf), " %.0f/%.0f MB ", data.gpu_vram_used, data.gpu_vram_total);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, rx + 50, ry);

    ry += 20;
    drawGraph(rx + 4, ry, GRAPH_W, GRAPH_H, _gpu_history, HISTORY_LEN, COL_GREEN);

    // ========== RAM Section ==========
    int ry2 = RAM_Y + 3;
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_YELLOW, COL_BG);
    _lcd->drawString("RAM", 6, ry2);

    uint16_t ramCol = (data.ram_percent < 70) ? COL_GREEN : (data.ram_percent < 90) ? COL_YELLOW : COL_RED;
    drawBar(44, ry2, 240, BAR_H, data.ram_percent, ramCol);

    snprintf(buf, sizeof(buf), "%.0f%%  %.1f/%.0fGB  ", data.ram_percent, data.ram_used_gb, data.ram_total_gb);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 290, ry2);

    // ========== Storage Section ==========
    int sy = STORAGE_Y + 3;
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

    // ========== Network Section ==========
    int ny = NET_Y + 3;
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString("NET", 6, ny);

    char dlBuf[16], ulBuf[16];
    formatSpeed(data.net_download, dlBuf, sizeof(dlBuf));
    formatSpeed(data.net_upload, ulBuf, sizeof(ulBuf));

    _lcd->setFont(&fonts::Font2);
    snprintf(buf, sizeof(buf), "DL: %s  ", dlBuf);
    _lcd->setTextColor(COL_GREEN, COL_BG);
    _lcd->drawString(buf, 44, ny);

    snprintf(buf, sizeof(buf), "UL: %s      ", ulBuf);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 210, ny);

    // ========== Bottom Section: Fans + Disk Temps ==========
    int by = BOTTOM_Y + 4;
    _lcd->setFont(&fonts::Font2);

    // Fans row
    for (int i = 0; i < 2; i++) {
        int fx = 6 + i * 150;
        if (data.fan[i] >= 0) {
            snprintf(buf, sizeof(buf), "FAN%d: %d RPM  ", i + 1, data.fan[i]);
            _lcd->setTextColor(COL_TEXT, COL_BG);
        } else {
            snprintf(buf, sizeof(buf), "FAN%d: ---      ", i + 1);
            _lcd->setTextColor(COL_DIVIDER, COL_BG);
        }
        _lcd->drawString(buf, fx, by);
    }

    // Disk temperatures - 2 columns, Font2, with °C
    int dty = by + 22;
    int col1x = 6;
    int col2x = SCREEN_W / 2 + 4;

    for (int i = 0; i < data.disk_count && i < 8; i++) {
        int cx = (i % 2 == 0) ? col1x : col2x;
        int cy = dty + (i / 2) * 20;

        if (cy + 16 > SCREEN_H) break;  // Don't draw outside screen

        _lcd->setFont(&fonts::Font2);
        // Disk name in gray
        snprintf(buf, sizeof(buf), "%-8s", data.disk_name[i]);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawString(buf, cx, cy);

        // Temp in color with °C
        if (data.disk_temp[i] >= 0) {
            snprintf(buf, sizeof(buf), "%d C  ", data.disk_temp[i]);
            _lcd->setTextColor(tempColor((float)data.disk_temp[i]), COL_BG);
        } else {
            snprintf(buf, sizeof(buf), "-- C  ");
            _lcd->setTextColor(COL_DIVIDER, COL_BG);
        }
        _lcd->drawString(buf, cx + 100, cy);
    }

    // "..." Advanced button (bottom right)
    drawAdvButton();

    // Connection dot (top right corner)
    _lcd->fillCircle(SCREEN_W - 8, 6, 4, data.connected ? COL_GREEN : COL_RED);
}


// ==================== CPU DETAIL ====================

void PCMonitorDisplay::drawCpuDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_CYAN, COL_BG);
        _lcd->drawCenterString("CPU", SCREEN_W / 2, 4);

        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawCenterString(data.cpu_name, SCREEN_W / 2, 30);

        _first_draw = false;
    }

    int y = 55;

    // Large load display
    snprintf(buf, sizeof(buf), "  %3.1f%%  ", data.cpu_load);
    _lcd->setFont(&fonts::Font4);
    uint16_t cpuCol = (data.cpu_load < 70) ? COL_GREEN : (data.cpu_load < 90) ? COL_YELLOW : COL_RED;
    _lcd->setTextColor(cpuCol, COL_BG);
    _lcd->drawString(buf, 10, y);

    // Large temp
    snprintf(buf, sizeof(buf), "  %.1f C  ", data.cpu_temp);
    _lcd->setTextColor(tempColor(data.cpu_temp), COL_BG);
    _lcd->drawString(buf, 200, y);

    y += 35;

    // Stats row
    _lcd->setFont(&fonts::Font2);
    snprintf(buf, sizeof(buf), "Clock:  %.0f MHz  ", data.cpu_clock);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 10, y);

    snprintf(buf, sizeof(buf), "Power:  %.0f W   ", data.cpu_power);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString(buf, 200, y);

    y += 20;

    snprintf(buf, sizeof(buf), "Voltage:  %.3f V  ", data.cpu_voltage);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, 10, y);

    y += 25;

    // Load bar (wide)
    drawBar(10, y, SCREEN_W - 20, 16, data.cpu_load, cpuCol);
    y += 22;

    // Graph (wide)
    drawGraph(10, y, SCREEN_W - 20, 70, _cpu_history, HISTORY_LEN, COL_CYAN);
    y += 78;

    // Per-core loads (compact bars)
    _lcd->setFont(&fonts::Font0);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Per-Core Load:", 10, y);
    y += 12;

    int coresPerRow = 8;
    int barW = (SCREEN_W - 20) / coresPerRow - 2;
    int barH2 = 8;

    for (int i = 0; i < data.cpu_core_count && i < 16; i++) {
        int col = i % coresPerRow;
        int row = i / coresPerRow;
        int bx = 10 + col * (barW + 2);
        int by = y + row * (barH2 + 10);

        float load = data.cpu_core_load[i];
        uint16_t cc = (load < 70) ? COL_GREEN : (load < 90) ? COL_YELLOW : COL_RED;
        drawBar(bx, by, barW, barH2, load, cc);

        // Core number below bar
        snprintf(buf, sizeof(buf), "%d", i + 1);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawCenterString(buf, bx + barW / 2, by + barH2 + 1);
    }
}


// ==================== GPU DETAIL ====================

void PCMonitorDisplay::drawGpuDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_GREEN, COL_BG);
        _lcd->drawCenterString("GPU", SCREEN_W / 2, 4);

        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawCenterString(data.gpu_name, SCREEN_W / 2, 30);

        _first_draw = false;
    }

    int y = 55;

    // Large load + temp
    snprintf(buf, sizeof(buf), "  %3.1f%%  ", data.gpu_load);
    _lcd->setFont(&fonts::Font4);
    uint16_t gpuCol = (data.gpu_load < 70) ? COL_GREEN : (data.gpu_load < 90) ? COL_YELLOW : COL_RED;
    _lcd->setTextColor(gpuCol, COL_BG);
    _lcd->drawString(buf, 10, y);

    snprintf(buf, sizeof(buf), "  %.1f C  ", data.gpu_temp);
    _lcd->setTextColor(tempColor(data.gpu_temp), COL_BG);
    _lcd->drawString(buf, 200, y);

    y += 35;

    // Stats
    _lcd->setFont(&fonts::Font2);

    snprintf(buf, sizeof(buf), "Core:  %.0f MHz   ", data.gpu_core_clock);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 10, y);

    snprintf(buf, sizeof(buf), "Mem:  %.0f MHz   ", data.gpu_mem_clock);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, 200, y);

    y += 20;

    snprintf(buf, sizeof(buf), "Power: %.0f W   ", data.gpu_power);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString(buf, 10, y);

    if (data.gpu_hotspot > 0) {
        snprintf(buf, sizeof(buf), "Hot Spot: %.0f C  ", data.gpu_hotspot);
        _lcd->setTextColor(tempColor(data.gpu_hotspot), COL_BG);
        _lcd->drawString(buf, 200, y);
    }

    y += 20;

    if (data.gpu_fan_rpm >= 0) {
        snprintf(buf, sizeof(buf), "Fan: %d RPM   ", data.gpu_fan_rpm);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawString(buf, 10, y);
    }

    y += 25;

    // VRAM bar
    float vramPct = 0;
    if (data.gpu_vram_total > 0)
        vramPct = (data.gpu_vram_used / data.gpu_vram_total) * 100.0f;
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("VRAM:", 10, y);
    drawBar(60, y, 300, 16, vramPct, COL_GREEN);
    snprintf(buf, sizeof(buf), "%.0f/%.0fMB ", data.gpu_vram_used, data.gpu_vram_total);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 370, y);

    y += 25;

    // Load bar
    drawBar(10, y, SCREEN_W - 20, 16, data.gpu_load, gpuCol);
    y += 22;

    // Graph
    drawGraph(10, y, SCREEN_W - 20, 70, _gpu_history, HISTORY_LEN, COL_GREEN);
}


// ==================== RAM DETAIL ====================

void PCMonitorDisplay::drawRamDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_YELLOW, COL_BG);
        _lcd->drawCenterString("RAM", SCREEN_W / 2, 4);

        _first_draw = false;
    }

    int y = 40;

    // Large percentage
    snprintf(buf, sizeof(buf), "  %.1f%%  ", data.ram_percent);
    _lcd->setFont(&fonts::Font4);
    uint16_t ramCol = (data.ram_percent < 70) ? COL_GREEN : (data.ram_percent < 90) ? COL_YELLOW : COL_RED;
    _lcd->setTextColor(ramCol, COL_BG);
    _lcd->drawCenterString(buf, SCREEN_W / 2, y);

    y += 40;

    // Used / Total / Free
    _lcd->setFont(&fonts::Font2);
    snprintf(buf, sizeof(buf), "Used: %.1f GB   ", data.ram_used_gb);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString(buf, 30, y);

    snprintf(buf, sizeof(buf), "Total: %.0f GB   ", data.ram_total_gb);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 250, y);

    y += 22;

    float freeGb = data.ram_total_gb - data.ram_used_gb;
    if (freeGb < 0) freeGb = 0;
    snprintf(buf, sizeof(buf), "Free: %.1f GB   ", freeGb);
    _lcd->setTextColor(COL_GREEN, COL_BG);
    _lcd->drawString(buf, 30, y);

    y += 30;

    // Wide bar
    drawBar(10, y, SCREEN_W - 20, 24, data.ram_percent, ramCol);

    y += 32;

    // Visual: used block vs free block
    int usedW = (int)((SCREEN_W - 20) * data.ram_percent / 100.0f);
    int freeW = SCREEN_W - 20 - usedW;
    _lcd->fillRect(10, y, usedW, 30, COL_ORANGE);
    if (freeW > 0) _lcd->fillRect(10 + usedW, y, freeW, 30, COL_GREEN);
    _lcd->drawRect(10, y, SCREEN_W - 20, 30, COL_DIVIDER);

    // Labels inside blocks
    _lcd->setFont(&fonts::Font2);
    if (usedW > 60) {
        snprintf(buf, sizeof(buf), "%.1fGB", data.ram_used_gb);
        _lcd->setTextColor(COL_BG, COL_ORANGE);
        _lcd->drawCenterString(buf, 10 + usedW / 2, y + 8);
    }
    if (freeW > 60) {
        snprintf(buf, sizeof(buf), "%.1fGB", freeGb);
        _lcd->setTextColor(COL_BG, COL_GREEN);
        _lcd->drawCenterString(buf, 10 + usedW + freeW / 2, y + 8);
    }

    y += 40;

    // Graph
    _lcd->setFont(&fonts::Font0);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Usage History (60s):", 10, y);
    y += 12;
    drawGraph(10, y, SCREEN_W - 20, 80, _ram_history, HISTORY_LEN, COL_YELLOW);
}


// ==================== DISK DETAIL ====================

void PCMonitorDisplay::drawDiskDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_BLUE, COL_BG);
        _lcd->drawCenterString("STORAGE", SCREEN_W / 2, 4);

        _first_draw = false;
    }

    int y = 35;

    // Total summary
    _lcd->setFont(&fonts::Font2);
    float sPct = 0;
    if (data.storage_total_tb > 0)
        sPct = (data.storage_used_tb / data.storage_total_tb) * 100.0f;
    snprintf(buf, sizeof(buf), "Total: %.1f TB Used / %.1f TB Free / %.1f TB Total   ",
             data.storage_used_tb, data.storage_free_tb, data.storage_total_tb);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 10, y);

    y += 22;

    // Total bar
    uint16_t sCol = (sPct < 70) ? COL_GREEN : (sPct < 90) ? COL_YELLOW : COL_RED;
    drawBar(10, y, SCREEN_W - 20, 20, sPct, sCol);
    snprintf(buf, sizeof(buf), "%.0f%%", sPct);
    _lcd->setTextColor(COL_BG, sCol);
    _lcd->drawCenterString(buf, SCREEN_W / 2, y + 3);

    y += 28;

    // Divider
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 4;

    // Per-disk list
    _lcd->setFont(&fonts::Font2);
    for (int i = 0; i < data.disk_count && i < 8 && y < 300; i++) {
        // Name
        _lcd->setTextColor(COL_TEXT, COL_BG);
        snprintf(buf, sizeof(buf), "%-10s", data.disk_name[i]);
        _lcd->drawString(buf, 10, y);

        // Size
        if (data.disk_size_gb[i] > 0) {
            if (data.disk_size_gb[i] >= 1000) {
                snprintf(buf, sizeof(buf), "%.1fTB ", data.disk_size_gb[i] / 1000.0f);
            } else {
                snprintf(buf, sizeof(buf), "%dGB ", data.disk_size_gb[i]);
            }
            _lcd->setTextColor(COL_LABEL, COL_BG);
            _lcd->drawString(buf, 120, y);
        }

        // Temp
        if (data.disk_temp[i] >= 0) {
            snprintf(buf, sizeof(buf), "%d C  ", data.disk_temp[i]);
            _lcd->setTextColor(tempColor((float)data.disk_temp[i]), COL_BG);
            _lcd->drawString(buf, 220, y);
        } else {
            _lcd->setTextColor(COL_DIVIDER, COL_BG);
            _lcd->drawString("-- C ", 220, y);
        }

        // Small bar showing relative size contribution
        if (data.disk_size_gb[i] > 0 && data.storage_total_tb > 0) {
            float diskPct = (data.disk_size_gb[i] / 1000.0f) / data.storage_total_tb * 100.0f;
            drawBar(280, y + 2, 190, 10, diskPct, COL_BLUE);
        }

        y += 22;
    }
}


// ==================== FAN DETAIL ====================

void PCMonitorDisplay::drawFanDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawCenterString("FANS", SCREEN_W / 2, 4);

        _first_draw = false;
    }

    int y = 45;

    // System fans (from mainboard)
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("System Fans (Mainboard)", 10, y);
    y += 22;

    for (int i = 0; i < 2; i++) {
        snprintf(buf, sizeof(buf), "FAN %d:", i + 1);
        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawString(buf, 20, y);

        if (data.fan[i] >= 0) {
            snprintf(buf, sizeof(buf), " %d RPM    ", data.fan[i]);
            // Color: green < 1000, yellow 1000-1500, red > 1500
            uint16_t fc = COL_GREEN;
            if (data.fan[i] > 1500) fc = COL_RED;
            else if (data.fan[i] > 1000) fc = COL_YELLOW;
            _lcd->setTextColor(fc, COL_BG);
            _lcd->drawString(buf, 140, y);

            // RPM bar (0-2500 RPM range)
            float fanPct = (data.fan[i] / 2500.0f) * 100.0f;
            if (fanPct > 100) fanPct = 100;
            drawBar(310, y + 5, 160, 16, fanPct, fc);
        } else {
            _lcd->setFont(&fonts::Font4);
            _lcd->setTextColor(COL_DIVIDER, COL_BG);
            _lcd->drawString(" ---       ", 140, y);
        }
        y += 40;
    }

    y += 10;

    // GPU fan
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 8;
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("GPU Fan", 10, y);
    y += 22;

    _lcd->setFont(&fonts::Font4);
    if (data.gpu_fan_rpm >= 0) {
        snprintf(buf, sizeof(buf), " %d RPM    ", data.gpu_fan_rpm);
        uint16_t gc = COL_GREEN;
        if (data.gpu_fan_rpm > 2000) gc = COL_RED;
        else if (data.gpu_fan_rpm > 1500) gc = COL_YELLOW;
        _lcd->setTextColor(gc, COL_BG);
        _lcd->drawString(buf, 20, y);

        float gpuFanPct = (data.gpu_fan_rpm / 3000.0f) * 100.0f;
        if (gpuFanPct > 100) gpuFanPct = 100;
        drawBar(250, y + 5, 220, 16, gpuFanPct, gc);
    } else {
        _lcd->setTextColor(COL_DIVIDER, COL_BG);
        _lcd->drawString(" Not available    ", 20, y);
    }

    y += 45;

    // Summary at bottom
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    int activeCount = 0;
    for (int i = 0; i < 2; i++) {
        if (data.fan[i] >= 0) activeCount++;
    }
    if (data.gpu_fan_rpm >= 0) activeCount++;
    snprintf(buf, sizeof(buf), "%d active fan(s) detected", activeCount);
    _lcd->drawCenterString(buf, SCREEN_W / 2, y);
}


// ==================== NETWORK DETAIL ====================

void PCMonitorDisplay::drawNetDetail(const HWData &data) {
    char buf[64];
    char speedBuf[16];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_ORANGE, COL_BG);
        _lcd->drawCenterString("NETWORK", SCREEN_W / 2, 4);

        // Static labels
        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawString("Download", 10, 56);
        _lcd->drawString("Upload", 250, 56);

        _first_draw = false;
    }

    int y = 34;

    // Adapter name (below title)
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_DIVIDER, COL_BG);
    snprintf(buf, sizeof(buf), "%-31s", data.net_adapter);
    _lcd->drawCenterString(buf, SCREEN_W / 2, y);

    y = 72;

    // === Download speed (left) + Upload speed (right) ===
    _lcd->setFont(&fonts::Font4);
    formatSpeed(data.net_download, speedBuf, sizeof(speedBuf));
    snprintf(buf, sizeof(buf), "%-14s", speedBuf);
    _lcd->setTextColor(COL_GREEN, COL_BG);
    _lcd->drawString(buf, 10, y);

    formatSpeed(data.net_upload, speedBuf, sizeof(speedBuf));
    snprintf(buf, sizeof(buf), "%-14s", speedBuf);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 250, y);

    y += 30;

    // === Download bar + Upload bar ===
    float maxDl = 1.0f, maxUl = 1.0f;
    for (int i = 0; i < HISTORY_LEN; i++) {
        if (_net_dl_history[i] > maxDl) maxDl = _net_dl_history[i];
        if (_net_ul_history[i] > maxUl) maxUl = _net_ul_history[i];
    }
    float dlPct = (maxDl > 0) ? (data.net_download / maxDl) * 100.0f : 0;
    float ulPct = (maxUl > 0) ? (data.net_upload / maxUl) * 100.0f : 0;
    if (dlPct > 100) dlPct = 100;
    if (ulPct > 100) ulPct = 100;
    drawBar(10, y, 220, 14, dlPct, COL_GREEN);
    drawBar(250, y, 220, 14, ulPct, COL_CYAN);

    y += 22;

    // === Utilization + Total Data row ===
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Utilization:", 10, y);
    snprintf(buf, sizeof(buf), "%.1f%%   ", data.net_util);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 110, y);

    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Total:", 250, y);
    snprintf(buf, sizeof(buf), "%.1f GB / %.1f GB   ", data.net_data_dl, data.net_data_up);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 300, y);

    y += 24;

    // === Download graph ===
    _lcd->setFont(&fonts::Font0);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    formatSpeed(maxDl, speedBuf, sizeof(speedBuf));
    snprintf(buf, sizeof(buf), "Download (peak: %s)   ", speedBuf);
    _lcd->drawString(buf, 10, y);
    y += 10;

    float normDl[60];
    for (int i = 0; i < HISTORY_LEN; i++) {
        normDl[i] = (maxDl > 0) ? (_net_dl_history[i] / maxDl) * 100.0f : 0;
    }
    drawGraph(10, y, SCREEN_W / 2 - 15, 55, normDl, HISTORY_LEN, COL_GREEN);

    // === Upload graph (right side, same row) ===
    _lcd->setFont(&fonts::Font0);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    formatSpeed(maxUl, speedBuf, sizeof(speedBuf));
    snprintf(buf, sizeof(buf), "Upload (peak: %s)   ", speedBuf);
    _lcd->drawString(buf, SCREEN_W / 2 + 5, y - 10);

    float normUl[60];
    for (int i = 0; i < HISTORY_LEN; i++) {
        normUl[i] = (maxUl > 0) ? (_net_ul_history[i] / maxUl) * 100.0f : 0;
    }
    drawGraph(SCREEN_W / 2 + 5, y, SCREEN_W / 2 - 15, 55, normUl, HISTORY_LEN, COL_CYAN);

    y += 63;

    // === Combined DL+UL graph (full width, bottom) ===
    _lcd->setFont(&fonts::Font0);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    float maxCombined = maxDl > maxUl ? maxDl : maxUl;
    formatSpeed(maxCombined, speedBuf, sizeof(speedBuf));
    snprintf(buf, sizeof(buf), "Combined Traffic (scale: %s)   ", speedBuf);
    _lcd->drawString(buf, 10, y);
    y += 10;

    // Overlay both on same graph
    float normDlC[60], normUlC[60];
    for (int i = 0; i < HISTORY_LEN; i++) {
        normDlC[i] = (maxCombined > 0) ? (_net_dl_history[i] / maxCombined) * 100.0f : 0;
        normUlC[i] = (maxCombined > 0) ? (_net_ul_history[i] / maxCombined) * 100.0f : 0;
    }
    drawGraph(10, y, SCREEN_W - 20, 55, normDlC, HISTORY_LEN, COL_GREEN);
    drawGraph(10, y, SCREEN_W - 20, 55, normUlC, HISTORY_LEN, COL_CYAN);
}


// ==================== ADVANCED MAIN SCREEN ====================

// Helper: draw a section header bar with colored background
static void drawSectionBar(lgfx::LGFX_Device *lcd, int x, int y, int w, const char *label, uint16_t accentCol) {
    lcd->fillRoundRect(x, y, w, 16, 3, accentCol);
    lcd->setFont(&fonts::Font0);
    lcd->setTextColor(COL_TEXT, accentCol);
    lcd->drawString(label, x + 4, y + 4);
}

void PCMonitorDisplay::drawAdvMainScreen(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawMainButton();

        // Title bar
        _lcd->fillRect(60, 2, SCREEN_W - 120, 24, COL_HEADER);
        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_TEXT, COL_HEADER);
        _lcd->drawCenterString("ADVANCED", SCREEN_W / 2, 6);

        // Section header bars
        drawSectionBar(_lcd, 4, 30, 230, "MOTHERBOARD", COL_DIVIDER);
        drawSectionBar(_lcd, 246, 30, 230, "CPU INTERNALS", COL_DIVIDER);
        drawSectionBar(_lcd, 4, 134, SCREEN_W - 8, "RAM / DIMM", COL_DIVIDER);
        drawSectionBar(_lcd, 4, 184, SCREEN_W - 8, "DISK I/O", COL_DIVIDER);
        drawSectionBar(_lcd, 4, 252, SCREEN_W - 8, "GPU", COL_DIVIDER);

        _first_draw = false;
    }

    // === Motherboard section (left, rows 48-130) ===
    int y = 50;
    _lcd->setFont(&fonts::Font2);

    // Voltages in a compact row
    snprintf(buf, sizeof(buf), "Vcore  ");
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, 8, y);
    snprintf(buf, sizeof(buf), "%.3fV ", data.mb_vcore);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 60, y);
    y += 16;

    snprintf(buf, sizeof(buf), "+3.3V  ");
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, 8, y);
    snprintf(buf, sizeof(buf), "%.2fV ", data.mb_33v);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 60, y);

    snprintf(buf, sizeof(buf), "CMOS ");
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, 130, y);
    snprintf(buf, sizeof(buf), "%.2fV ", data.mb_cmos);
    uint16_t cmCol = (data.mb_cmos > 2.7f) ? COL_GREEN : COL_RED;
    _lcd->setTextColor(cmCol, COL_BG);
    _lcd->drawString(buf, 170, y);
    y += 18;

    // Board temps with mini bars
    for (int i = 0; i < data.mb_temp_count && i < 4; i++) {
        snprintf(buf, sizeof(buf), "%-6s", data.mb_tnames[i]);
        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawString(buf, 8, y);
        snprintf(buf, sizeof(buf), "%dC ", data.mb_temps[i]);
        _lcd->setTextColor(tempColor((float)data.mb_temps[i]), COL_BG);
        _lcd->drawString(buf, 65, y);
        // Mini temp bar
        float tPct = (data.mb_temps[i] / 100.0f) * 100.0f;
        if (tPct > 100) tPct = 100;
        drawBar(110, y + 3, 120, 8, tPct, tempColor((float)data.mb_temps[i]));
        y += 15;
    }

    // === CPU Internals section (right, rows 48-130) ===
    int rx = 250;
    y = 50;

    // CCD temps side by side with bars
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("CCD1", rx, y);
    snprintf(buf, sizeof(buf), "%.0fC ", data.cpu_ccd1_temp);
    _lcd->setTextColor(tempColor(data.cpu_ccd1_temp), COL_BG);
    _lcd->drawString(buf, rx + 40, y);
    drawBar(rx + 80, y + 3, 60, 8, (data.cpu_ccd1_temp / 100.0f) * 100.0f, tempColor(data.cpu_ccd1_temp));

    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("CCD2", rx + 150, y);
    snprintf(buf, sizeof(buf), "%.0fC ", data.cpu_ccd2_temp);
    _lcd->setTextColor(tempColor(data.cpu_ccd2_temp), COL_BG);
    _lcd->drawString(buf, rx + 190, y);
    y += 16;

    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("SoC", rx, y);
    snprintf(buf, sizeof(buf), "%.0fC ", data.cpu_soc_temp);
    _lcd->setTextColor(tempColor(data.cpu_soc_temp), COL_BG);
    _lcd->drawString(buf, rx + 40, y);
    y += 18;

    // TDC with bar
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("TDC", rx, y);
    snprintf(buf, sizeof(buf), "%.1fA ", data.cpu_tdc);
    uint16_t tdcCol = (data.cpu_tdc < 80) ? COL_GREEN : (data.cpu_tdc < 120) ? COL_YELLOW : COL_RED;
    _lcd->setTextColor(tdcCol, COL_BG);
    _lcd->drawString(buf, rx + 40, y);
    float tdcPct = (data.cpu_tdc / 160.0f) * 100.0f;
    if (tdcPct > 100) tdcPct = 100;
    drawBar(rx + 100, y + 3, 126, 8, tdcPct, tdcCol);
    y += 18;

    // Clocks
    snprintf(buf, sizeof(buf), "Fab %.0f  Mem %.0f MHz ", data.cpu_fabric_clk, data.cpu_mem_clk);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, rx, y);
    y += 16;

    snprintf(buf, sizeof(buf), "Bus %.1f MHz ", data.cpu_bus_speed);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, rx, y);

    // === RAM / DIMM section (rows 150-180) ===
    y = 153;
    _lcd->setFont(&fonts::Font2);

    // DIMM temps as horizontal boxes
    for (int i = 0; i < data.dimm_count && i < 4; i++) {
        int dx = 8 + i * 118;
        // Small colored box
        uint16_t dc = tempColor((float)data.dimm_temps[i]);
        _lcd->fillRoundRect(dx, y, 110, 24, 3, COL_BAR_BG);
        _lcd->drawRoundRect(dx, y, 110, 24, 3, dc);
        snprintf(buf, sizeof(buf), "D%d", i + 1);
        _lcd->setTextColor(COL_LABEL, COL_BAR_BG);
        _lcd->drawString(buf, dx + 4, y + 5);
        snprintf(buf, sizeof(buf), "%dC", data.dimm_temps[i]);
        _lcd->setTextColor(dc, COL_BAR_BG);
        _lcd->drawString(buf, dx + 36, y + 5);
        // Mini bar inside box
        float dimPct = (data.dimm_temps[i] / 80.0f) * 100.0f;
        drawBar(dx + 72, y + 7, 34, 8, dimPct, dc);
    }

    // === Disk I/O section (rows 202-248) ===
    y = 203;
    _lcd->setFont(&fonts::Font2);

    for (int i = 0; i < data.disk_count && i < 4; i++) {
        char rdBuf[12], wrBuf[12];
        formatSpeed(data.disk_read[i], rdBuf, sizeof(rdBuf));
        formatSpeed(data.disk_write[i], wrBuf, sizeof(wrBuf));

        // Disk name
        snprintf(buf, sizeof(buf), "%-7s", data.disk_name[i]);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawString(buf, 8, y);

        // R/W speeds
        _lcd->setFont(&fonts::Font0);
        _lcd->setTextColor(COL_GREEN, COL_BG);
        snprintf(buf, sizeof(buf), "R:%-9s", rdBuf);
        _lcd->drawString(buf, 80, y + 1);
        _lcd->setTextColor(COL_ORANGE, COL_BG);
        snprintf(buf, sizeof(buf), "W:%-9s", wrBuf);
        _lcd->drawString(buf, 170, y + 1);
        _lcd->setFont(&fonts::Font2);

        // Activity bar (remaining width)
        drawBar(270, y + 2, 140, 10, data.disk_act[i], COL_BLUE);
        snprintf(buf, sizeof(buf), "%.1f%%", data.disk_act[i]);
        _lcd->setFont(&fonts::Font0);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawString(buf, 416, y + 2);
        _lcd->setFont(&fonts::Font2);

        // Temp
        if (data.disk_temp[i] >= 0) {
            snprintf(buf, sizeof(buf), "%dC", data.disk_temp[i]);
            _lcd->setTextColor(tempColor((float)data.disk_temp[i]), COL_BG);
            _lcd->drawString(buf, 450, y);
        }

        y += 12;
    }

    // === GPU Advanced section (rows 268-316) ===
    y = 268;
    _lcd->setFont(&fonts::Font2);

    // GPU row 1: Voltage + Power + D3D
    snprintf(buf, sizeof(buf), "%.3fV ", data.gpu_volt);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 8, y);

    snprintf(buf, sizeof(buf), "%.0fW ", data.gpu_board_pwr);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString(buf, 80, y);

    snprintf(buf, sizeof(buf), "Fan:%d%% ", (int)data.gpu_fan_ctrl);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 135, y);

    // D3D bar
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("D3D", 230, y);
    drawBar(270, y + 3, 120, 10, data.gpu_d3d_3d, COL_GREEN);
    snprintf(buf, sizeof(buf), "%.0f%%", data.gpu_d3d_3d);
    _lcd->setTextColor(COL_GREEN, COL_BG);
    _lcd->drawString(buf, 396, y);
    y += 16;

    // GPU row 2: PCIe + MC/VE
    char rxBuf[16], txBuf[16];
    formatSpeed(data.gpu_pcie_rx, rxBuf, sizeof(rxBuf));
    formatSpeed(data.gpu_pcie_tx, txBuf, sizeof(txBuf));
    _lcd->setFont(&fonts::Font0);
    snprintf(buf, sizeof(buf), "PCIe Rx:%s  Tx:%s", rxBuf, txBuf);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 8, y + 2);

    snprintf(buf, sizeof(buf), "MC:%.0f%% VE:%.0f%% Bus:%.0f%%", data.gpu_mem_ctrl_load, data.gpu_vid_eng_load, data.gpu_bus_load);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, 270, y + 2);

    // GPU row 3: D3D memory
    y += 14;
    snprintf(buf, sizeof(buf), "D3D Mem: %.0fMB ded  %.0fMB shared   VDec:%.0f%% VEnc:%.0f%%", data.gpu_dmem, data.gpu_smem, data.gpu_d3d_vdec, data.gpu_d3d_venc);
    _lcd->drawString(buf, 8, y + 2);

    // Connection dot
    _lcd->fillCircle(SCREEN_W - 8, 6, 4, data.connected ? COL_GREEN : COL_RED);
}


// ==================== ADV MOTHERBOARD DETAIL ====================

void PCMonitorDisplay::drawAdvMoboDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_MAGENTA, COL_BG);
        _lcd->drawCenterString("MOTHERBOARD", SCREEN_W / 2, 4);

        _first_draw = false;
    }

    int y = 40;

    // Voltages section
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Voltages", 10, y);
    y += 22;

    _lcd->setFont(&fonts::Font4);
    snprintf(buf, sizeof(buf), "Vcore: %.3f V  ", data.mb_vcore);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 20, y);
    y += 32;

    _lcd->setFont(&fonts::Font2);
    snprintf(buf, sizeof(buf), "+3.3V: %.2f V   ", data.mb_33v);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, 20, y);

    snprintf(buf, sizeof(buf), "CMOS Batt: %.2f V   ", data.mb_cmos);
    uint16_t cmosCol = (data.mb_cmos > 2.7f) ? COL_GREEN : (data.mb_cmos > 2.5f) ? COL_YELLOW : COL_RED;
    _lcd->setTextColor(cmosCol, COL_BG);
    _lcd->drawString(buf, 220, y);
    y += 28;

    // Divider
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 8;

    // Board temperatures
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Board Temperatures", 10, y);
    y += 22;

    for (int i = 0; i < data.mb_temp_count && i < 6; i++) {
        int cx = (i % 2 == 0) ? 20 : 250;
        int cy = y + (i / 2) * 28;

        snprintf(buf, sizeof(buf), "%-8s", data.mb_tnames[i]);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawString(buf, cx, cy);

        snprintf(buf, sizeof(buf), "%d C   ", data.mb_temps[i]);
        _lcd->setTextColor(tempColor((float)data.mb_temps[i]), COL_BG);
        _lcd->drawString(buf, cx + 90, cy);

        // Temp bar
        float tPct = (data.mb_temps[i] / 100.0f) * 100.0f;
        if (tPct > 100) tPct = 100;
        drawBar(cx + 140, cy + 2, 80, 12, tPct, tempColor((float)data.mb_temps[i]));
    }

    y += ((data.mb_temp_count + 1) / 2) * 28 + 8;

    // Fan Control
    if (y < 260) {
        _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
        y += 8;

        _lcd->setTextColor(COL_LABEL, COL_BG);
        _lcd->drawString("Fan Control", 10, y);
        y += 22;

        for (int i = 0; i < data.mb_fan_ctrl_count && i < 4 && y < 310; i++) {
            snprintf(buf, sizeof(buf), "Fan %d:", i + 1);
            _lcd->setTextColor(COL_TEXT, COL_BG);
            _lcd->drawString(buf, 20, y);

            snprintf(buf, sizeof(buf), "%d%%  ", data.mb_fan_ctrl[i]);
            _lcd->drawString(buf, 80, y);

            drawBar(120, y + 2, 340, 12, (float)data.mb_fan_ctrl[i], COL_CYAN);
            y += 20;
        }
    }
}


// ==================== ADV CPU DETAIL ====================

void PCMonitorDisplay::drawAdvCpuDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_CYAN, COL_BG);
        _lcd->drawCenterString("CPU ADV", SCREEN_W / 2, 4);

        _first_draw = false;
    }

    int y = 38;

    // Die temperatures
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Die Temperatures", 10, y);
    y += 22;

    _lcd->setFont(&fonts::Font4);
    snprintf(buf, sizeof(buf), "CCD1: %.0f C  ", data.cpu_ccd1_temp);
    _lcd->setTextColor(tempColor(data.cpu_ccd1_temp), COL_BG);
    _lcd->drawString(buf, 20, y);

    snprintf(buf, sizeof(buf), "CCD2: %.0f C  ", data.cpu_ccd2_temp);
    _lcd->setTextColor(tempColor(data.cpu_ccd2_temp), COL_BG);
    _lcd->drawString(buf, 250, y);
    y += 32;

    _lcd->setFont(&fonts::Font2);
    snprintf(buf, sizeof(buf), "SoC: %.0f C  ", data.cpu_soc_temp);
    _lcd->setTextColor(tempColor(data.cpu_soc_temp), COL_BG);
    _lcd->drawString(buf, 20, y);

    snprintf(buf, sizeof(buf), "Tctl: %.1f C  ", data.cpu_temp);
    _lcd->setTextColor(tempColor(data.cpu_temp), COL_BG);
    _lcd->drawString(buf, 200, y);
    y += 28;

    // Divider
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 8;

    // TDC Current
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("TDC Current", 10, y);
    y += 20;

    _lcd->setFont(&fonts::Font4);
    snprintf(buf, sizeof(buf), "%.1f A    ", data.cpu_tdc);
    uint16_t tdcCol = (data.cpu_tdc < 80) ? COL_GREEN : (data.cpu_tdc < 120) ? COL_YELLOW : COL_RED;
    _lcd->setTextColor(tdcCol, COL_BG);
    _lcd->drawString(buf, 20, y);

    // TDC bar (assume 160A max for 5950X)
    float tdcPct = (data.cpu_tdc / 160.0f) * 100.0f;
    if (tdcPct > 100) tdcPct = 100;
    drawBar(180, y + 5, 280, 16, tdcPct, tdcCol);
    y += 32;

    // Divider
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 8;

    // Clocks
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Internal Clocks", 10, y);
    y += 22;

    snprintf(buf, sizeof(buf), "Fabric: %.0f MHz   ", data.cpu_fabric_clk);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 20, y);

    snprintf(buf, sizeof(buf), "Memory: %.0f MHz   ", data.cpu_mem_clk);
    _lcd->setTextColor(COL_GREEN, COL_BG);
    _lcd->drawString(buf, 250, y);
    y += 20;

    snprintf(buf, sizeof(buf), "Bus Speed: %.1f MHz   ", data.cpu_bus_speed);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, 20, y);

    snprintf(buf, sizeof(buf), "Avg Clock: %.0f MHz   ", data.cpu_clock);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString(buf, 250, y);
    y += 28;

    // Power section
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 8;

    snprintf(buf, sizeof(buf), "Package: %.0f W   Voltage: %.3f V   ", data.cpu_power, data.cpu_voltage);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString(buf, 10, y);
}


// ==================== ADV GPU DETAIL ====================

void PCMonitorDisplay::drawAdvGpuDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_GREEN, COL_BG);
        _lcd->drawCenterString("GPU ADV", SCREEN_W / 2, 4);

        _first_draw = false;
    }

    int y = 38;

    // Voltage + Board Power
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Power & Voltage", 10, y);
    y += 20;

    snprintf(buf, sizeof(buf), "Core: %.3f V  ", data.gpu_volt);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 20, y);

    snprintf(buf, sizeof(buf), "Board Pwr: %.0f W  ", data.gpu_board_pwr);
    _lcd->setTextColor(COL_ORANGE, COL_BG);
    _lcd->drawString(buf, 200, y);

    snprintf(buf, sizeof(buf), "Fan: %d%%  ", (int)data.gpu_fan_ctrl);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 400, y);
    y += 26;

    // Divider
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 6;

    // PCIe Throughput
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("PCIe Throughput", 10, y);
    y += 20;

    char rxBuf[16], txBuf[16];
    formatSpeed(data.gpu_pcie_rx, rxBuf, sizeof(rxBuf));
    formatSpeed(data.gpu_pcie_tx, txBuf, sizeof(txBuf));

    snprintf(buf, sizeof(buf), "Rx: %s  ", rxBuf);
    _lcd->setTextColor(COL_GREEN, COL_BG);
    _lcd->drawString(buf, 20, y);
    drawBar(150, y + 2, 80, 12, 0, COL_GREEN);  // placeholder

    snprintf(buf, sizeof(buf), "Tx: %s  ", txBuf);
    _lcd->setTextColor(COL_CYAN, COL_BG);
    _lcd->drawString(buf, 260, y);
    drawBar(380, y + 2, 80, 12, 0, COL_CYAN);
    y += 24;

    // Divider
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 6;

    // D3D Engine Loads
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("D3D Engine Loads", 10, y);
    y += 20;

    // 3D bar
    snprintf(buf, sizeof(buf), "3D: %.0f%%  ", data.gpu_d3d_3d);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 20, y);
    drawBar(100, y + 2, 370, 14, data.gpu_d3d_3d, COL_GREEN);
    y += 20;

    // Copy bar
    snprintf(buf, sizeof(buf), "Copy: %.0f%%", data.gpu_d3d_copy);
    _lcd->drawString(buf, 20, y);
    drawBar(100, y + 2, 370, 14, data.gpu_d3d_copy, COL_CYAN);
    y += 20;

    // Video Decode
    snprintf(buf, sizeof(buf), "VDec: %.0f%%", data.gpu_d3d_vdec);
    _lcd->drawString(buf, 20, y);
    drawBar(100, y + 2, 370, 14, data.gpu_d3d_vdec, COL_ORANGE);
    y += 20;

    // Video Encode
    snprintf(buf, sizeof(buf), "VEnc: %.0f%%", data.gpu_d3d_venc);
    _lcd->drawString(buf, 20, y);
    drawBar(100, y + 2, 370, 14, data.gpu_d3d_venc, COL_YELLOW);
    y += 26;

    // Divider
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 6;

    // Subsystem loads
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Subsystem Loads", 10, y);
    y += 20;

    snprintf(buf, sizeof(buf), "MemCtrl: %.0f%%  ", data.gpu_mem_ctrl_load);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 20, y);

    snprintf(buf, sizeof(buf), "VidEng: %.0f%%  ", data.gpu_vid_eng_load);
    _lcd->drawString(buf, 170, y);

    snprintf(buf, sizeof(buf), "Bus: %.0f%%  ", data.gpu_bus_load);
    _lcd->drawString(buf, 310, y);
    y += 20;

    // D3D Memory
    snprintf(buf, sizeof(buf), "D3D Ded: %.0fMB  Shared: %.0fMB  ", data.gpu_dmem, data.gpu_smem);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString(buf, 20, y);
}


// ==================== ADV RAM DETAIL ====================

void PCMonitorDisplay::drawAdvRamDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_YELLOW, COL_BG);
        _lcd->drawCenterString("RAM ADV", SCREEN_W / 2, 4);

        _first_draw = false;
    }

    int y = 40;

    // DIMM Temperatures
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("DIMM Temperatures", 10, y);
    y += 24;

    for (int i = 0; i < data.dimm_count && i < 4; i++) {
        int barX = 20;
        int barW = SCREEN_W - 40;

        _lcd->setFont(&fonts::Font4);
        snprintf(buf, sizeof(buf), "DIMM %d", i + 1);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawString(buf, barX, y);

        snprintf(buf, sizeof(buf), "%d C  ", data.dimm_temps[i]);
        _lcd->setTextColor(tempColor((float)data.dimm_temps[i]), COL_BG);
        _lcd->drawString(buf, 140, y);

        // Temp bar (0-80°C range)
        float dimPct = (data.dimm_temps[i] / 80.0f) * 100.0f;
        if (dimPct > 100) dimPct = 100;
        drawBar(220, y + 5, barW - 200, 18, dimPct, tempColor((float)data.dimm_temps[i]));

        y += 38;
    }

    if (data.dimm_count == 0) {
        _lcd->setFont(&fonts::Font2);
        _lcd->setTextColor(COL_DIVIDER, COL_BG);
        _lcd->drawString("No DIMM temperature data available", 20, y);
        y += 30;
    }

    y += 8;
    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 10;

    // Virtual Memory
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Virtual Memory", 10, y);
    y += 22;

    if (data.vm_total > 0) {
        snprintf(buf, sizeof(buf), "Used: %.1f GB  ", data.vm_used);
        _lcd->setTextColor(COL_ORANGE, COL_BG);
        _lcd->drawString(buf, 20, y);

        snprintf(buf, sizeof(buf), "Total: %.0f GB  ", data.vm_total);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawString(buf, 200, y);
        y += 22;

        float vmPct = (data.vm_used / data.vm_total) * 100.0f;
        drawBar(20, y, SCREEN_W - 40, 20, vmPct, COL_YELLOW);
        snprintf(buf, sizeof(buf), "%.0f%%", vmPct);
        _lcd->setTextColor(COL_BG, COL_YELLOW);
        _lcd->drawCenterString(buf, SCREEN_W / 2, y + 3);
        y += 28;
    }

    // Physical RAM comparison
    y += 4;
    _lcd->setFont(&fonts::Font2);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Physical Memory", 10, y);
    y += 22;

    snprintf(buf, sizeof(buf), "Used: %.1f GB / %.0f GB  (%.0f%%)  ",
             data.ram_used_gb, data.ram_total_gb, data.ram_percent);
    _lcd->setTextColor(COL_TEXT, COL_BG);
    _lcd->drawString(buf, 20, y);
    y += 20;
    drawBar(20, y, SCREEN_W - 40, 16, data.ram_percent,
            (data.ram_percent < 70) ? COL_GREEN : (data.ram_percent < 90) ? COL_YELLOW : COL_RED);
}


// ==================== ADV DISK I/O DETAIL ====================

void PCMonitorDisplay::drawAdvDiskDetail(const HWData &data) {
    char buf[64];

    if (_first_draw) {
        _lcd->fillScreen(COL_BG);
        drawBackButton();
        drawNextButton();

        _lcd->setFont(&fonts::Font4);
        _lcd->setTextColor(COL_BLUE, COL_BG);
        _lcd->drawCenterString("DISK I/O", SCREEN_W / 2, 4);

        _first_draw = false;
    }

    int y = 36;

    // Column headers
    _lcd->setFont(&fonts::Font0);
    _lcd->setTextColor(COL_LABEL, COL_BG);
    _lcd->drawString("Drive", 10, y);
    _lcd->drawString("Read", 90, y);
    _lcd->drawString("Write", 190, y);
    _lcd->drawString("Activity", 290, y);
    _lcd->drawString("Temp", 430, y);
    y += 14;

    _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
    y += 4;

    _lcd->setFont(&fonts::Font2);

    for (int i = 0; i < data.disk_count && i < 8 && y < 300; i++) {
        // Drive name
        snprintf(buf, sizeof(buf), "%-7s", data.disk_name[i]);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawString(buf, 10, y);

        // Read speed
        char rdBuf[14];
        formatSpeed(data.disk_read[i], rdBuf, sizeof(rdBuf));
        snprintf(buf, sizeof(buf), "%-10s", rdBuf);
        _lcd->setTextColor(COL_GREEN, COL_BG);
        _lcd->drawString(buf, 80, y);

        // Write speed
        char wrBuf[14];
        formatSpeed(data.disk_write[i], wrBuf, sizeof(wrBuf));
        snprintf(buf, sizeof(buf), "%-10s", wrBuf);
        _lcd->setTextColor(COL_ORANGE, COL_BG);
        _lcd->drawString(buf, 180, y);

        // Activity bar
        drawBar(280, y + 2, 100, 12, data.disk_act[i], COL_BLUE);
        snprintf(buf, sizeof(buf), "%.1f%%", data.disk_act[i]);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawString(buf, 385, y);

        // Temperature
        if (data.disk_temp[i] >= 0) {
            snprintf(buf, sizeof(buf), "%dC ", data.disk_temp[i]);
            _lcd->setTextColor(tempColor((float)data.disk_temp[i]), COL_BG);
        } else {
            snprintf(buf, sizeof(buf), "-- ");
            _lcd->setTextColor(COL_DIVIDER, COL_BG);
        }
        _lcd->drawString(buf, 440, y);

        y += 22;
    }

    // Summary
    if (y < 290) {
        y += 4;
        _lcd->drawFastHLine(10, y, SCREEN_W - 20, COL_DIVIDER);
        y += 8;

        _lcd->setFont(&fonts::Font2);
        float sPct = 0;
        if (data.storage_total_tb > 0)
            sPct = (data.storage_used_tb / data.storage_total_tb) * 100.0f;
        snprintf(buf, sizeof(buf), "Total: %.1f/%.1f TB (%.0f%% used)  Free: %.1f TB  ",
                 data.storage_used_tb, data.storage_total_tb, sPct, data.storage_free_tb);
        _lcd->setTextColor(COL_TEXT, COL_BG);
        _lcd->drawString(buf, 10, y);
    }
}
