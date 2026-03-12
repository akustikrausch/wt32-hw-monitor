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
    float cpu_clock;     // CPU average clock MHz
    float cpu_power;     // CPU package power W
    float gpu_vram_used; // GPU VRAM used MB
    float gpu_vram_total;// GPU VRAM total MB
    int   fan[2];        // Fan RPMs (-1 = not present), only 2
    float storage_total_tb;
    float storage_used_tb;
    float storage_free_tb;
    int   disk_temp[8];  // Disk temps in °C (-1 = not present)
    char  disk_name[8][12]; // Short disk names (e.g. "980PRO", "WD10T")
    int   disk_count;    // How many disks
    char  cpu_name[32];
    char  gpu_name[32];
    bool  connected;
};

// Display class for WT32-SC01 (ST7796S 480x320)
class PCMonitorDisplay {
public:
    void init();
    void update(const HWData &data);
    void showWaiting();

private:
    void drawBar(int x, int y, int w, int h, float percent, uint16_t color);
    void drawGraph(int x, int y, int w, int h, float *history, int len, uint16_t color);
    uint16_t tempColor(float temp);

    lgfx::LGFX_Device *_lcd;
    lgfx::LGFX_Sprite *_sprite;

    // History ring buffers
    float _cpu_history[60];
    float _gpu_history[60];
    int   _history_idx;
    bool  _first_draw;
};

extern PCMonitorDisplay display;

#endif
