#ifndef DISPLAY_H
#define DISPLAY_H

#include <LovyanGFX.hpp>

// Hardware data structure
struct HWData {
    float cpu_load;      // CPU usage %
    float gpu_load;      // GPU usage %
    float ram_percent;   // RAM usage %
    float ram_used_gb;   // RAM used in GB
    float ram_total_gb;  // RAM total in GB
    float cpu_temp;      // CPU temperature °C
    float gpu_temp;      // GPU temperature °C
    int   fan[4];        // Fan RPMs (-1 = not present)
    float storage_total_tb;  // Total storage in TB
    float storage_used_tb;   // Used storage in TB
    float storage_free_tb;   // Free storage in TB
    char  cpu_name[32];  // CPU model name
    char  gpu_name[32];  // GPU model name
    bool  connected;     // Data received recently
};

// Display class for WT32-SC01 (ST7796S 480x320)
class PCMonitorDisplay {
public:
    void init();
    void drawStatic();                    // Draw static elements (labels, dividers)
    void update(const HWData &data);      // Update dynamic values
    void showWaiting();                   // Show "waiting for connection" screen

private:
    void drawHeader();
    void drawBar(int x, int y, int w, int h, float percent, uint16_t color);
    void drawGraph(int x, int y, int w, int h, float *history, int len, uint16_t color);
    void drawTemp(int x, int y, float temp);
    void drawFanSection(const HWData &data);
    void drawStorageSection(const HWData &data);
    uint16_t tempColor(float temp);

    lgfx::LGFX_Device *_lcd;
    lgfx::LGFX_Sprite *_sprite;  // Double-buffered sprite for flicker-free updates

    // History ring buffers
    float _cpu_history[60];
    float _gpu_history[60];
    int   _history_idx;
    bool  _first_draw;
};

extern PCMonitorDisplay display;

#endif
