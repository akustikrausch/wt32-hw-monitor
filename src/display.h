#ifndef DISPLAY_H
#define DISPLAY_H

#include <LovyanGFX.hpp>

// Screen states for touch navigation
enum ScreenState {
    SCREEN_MAIN = 0,
    SCREEN_CPU_DETAIL,
    SCREEN_GPU_DETAIL,
    SCREEN_RAM_DETAIL,
    SCREEN_DISK_DETAIL,
    SCREEN_FAN_DETAIL,
    SCREEN_NET_DETAIL,
    // Advanced screens
    SCREEN_ADV_MAIN,
    SCREEN_ADV_MOBO,
    SCREEN_ADV_CPU,
    SCREEN_ADV_GPU,
    SCREEN_ADV_RAM,
    SCREEN_ADV_DISK,
    SCREEN_STANDBY,
};

// Hardware data structure
struct HWData {
    // Main screen data
    float cpu_load;       // CPU usage %
    float gpu_load;       // GPU usage %
    float ram_percent;    // RAM usage %
    float ram_used_gb;    // RAM used in GB
    float ram_total_gb;   // RAM total in GB
    float cpu_temp;       // CPU temperature C
    float gpu_temp;       // GPU temperature C
    float cpu_clock;      // CPU average clock MHz
    float cpu_power;      // CPU package power W
    float cpu_voltage;    // CPU core voltage V
    float gpu_vram_used;  // GPU VRAM used MB
    float gpu_vram_total; // GPU VRAM total MB
    int   fan[2];         // Fan RPMs (-1 = not present)
    float storage_total_tb;
    float storage_used_tb;
    float storage_free_tb;
    int   disk_temp[8];   // Disk temps in C (-1 = not present)
    char  disk_name[8][12]; // Short disk names
    int   disk_size_gb[8];  // Disk total size in GB
    int   disk_count;     // How many disks
    float net_download;   // Network download KB/s
    float net_upload;     // Network upload KB/s
    float net_util;       // Network utilization %
    float net_data_up;    // Total data uploaded GB (session)
    float net_data_dl;    // Total data downloaded GB (session)
    char  net_adapter[32]; // Active network adapter name

    // Advanced: Motherboard
    float mb_vcore;       // Vcore voltage
    float mb_33v;         // +3.3V rail
    float mb_cmos;        // CMOS battery voltage
    int   mb_temps[6];    // Board temperatures
    char  mb_tnames[6][9];// Board temp sensor names
    int   mb_temp_count;  // Number of board temps
    int   mb_fan_ctrl[4]; // Fan control percentages
    int   mb_fan_ctrl_count;

    // Advanced: CPU
    float cpu_soc_temp;   // SoC temperature
    float cpu_ccd1_temp;  // CCD1 die temperature
    float cpu_ccd2_temp;  // CCD2 die temperature
    float cpu_tdc;        // TDC current (Amps)
    float cpu_fabric_clk; // Fabric clock MHz
    float cpu_bus_speed;  // Bus speed MHz
    float cpu_mem_clk;    // Memory clock MHz

    // Advanced: GPU
    float gpu_volt;       // Core voltage
    float gpu_mem_ctrl_load; // Memory controller load %
    float gpu_vid_eng_load;  // Video engine load %
    float gpu_bus_load;   // Bus load %
    float gpu_board_pwr;  // Board power W
    float gpu_fan_ctrl;   // Fan control %
    float gpu_pcie_rx;    // PCIe Rx KB/s
    float gpu_pcie_tx;    // PCIe Tx KB/s
    float gpu_d3d_3d;     // D3D 3D load %
    float gpu_d3d_copy;   // D3D Copy load %
    float gpu_d3d_vdec;   // D3D Video Decode %
    float gpu_d3d_venc;   // D3D Video Encode %
    float gpu_dmem;       // D3D dedicated memory MB
    float gpu_smem;       // D3D shared memory MB

    // Advanced: RAM
    int   dimm_temps[4];  // Per-DIMM temperatures
    int   dimm_count;     // Number of DIMMs with temp
    float vm_used;        // Virtual memory used GB
    float vm_total;       // Virtual memory total GB

    // Advanced: Disk I/O
    float disk_read[8];   // Per-disk read KB/s
    float disk_write[8];  // Per-disk write KB/s
    float disk_act[8];    // Per-disk activity %

    // Detail data
    float gpu_core_clock; // GPU core clock MHz
    float gpu_mem_clock;  // GPU memory clock MHz
    float gpu_power;      // GPU power W
    float gpu_hotspot;    // GPU hot spot temp C
    int   gpu_fan_rpm;    // GPU fan RPM
    float cpu_core_load[16]; // Per-core CPU loads %
    int   cpu_core_count; // Number of CPU cores

    // Time sync (from PC)
    unsigned long pc_timestamp;   // Unix epoch seconds (UTC)
    int           tz_offset;      // Timezone offset in seconds (e.g. 3600=CET, 7200=CEST)

    char  cpu_name[32];
    char  gpu_name[32];
    bool  connected;
};

// Display class for WT32-SC01 (ST7796S 480x320)
class PCMonitorDisplay {
public:
    void init();
    void update(const HWData &data);
    void showStandby();
    void updateStandby();
    void handleTouch(const HWData &data);
    ScreenState getScreen() { return _screen; }
    bool isStandby() { return _screen == SCREEN_STANDBY; }

private:
    void drawBar(int x, int y, int w, int h, float percent, uint16_t color);
    void drawGraph(int x, int y, int w, int h, float *history, int len, uint16_t color);
    uint16_t tempColor(float temp);
    void drawMainScreen(const HWData &data);
    void drawCpuDetail(const HWData &data);
    void drawGpuDetail(const HWData &data);
    void drawRamDetail(const HWData &data);
    void drawDiskDetail(const HWData &data);
    void drawFanDetail(const HWData &data);
    void drawNetDetail(const HWData &data);
    void drawBackButton();
    void drawNextButton();
    void drawAdvButton();
    void drawMainButton();
    ScreenState nextDetailScreen(ScreenState current);
    ScreenState nextAdvDetailScreen(ScreenState current);
    void drawStandbyScreen();
    void drawAdvMainScreen(const HWData &data);
    void drawAdvMoboDetail(const HWData &data);
    void drawAdvCpuDetail(const HWData &data);
    void drawAdvGpuDetail(const HWData &data);
    void drawAdvRamDetail(const HWData &data);
    void drawAdvDiskDetail(const HWData &data);
    void drawCurrentScreen(const HWData &data);

    lgfx::LGFX_Device *_lcd;
    lgfx::LGFX_Sprite *_sprite;

    // History ring buffers
    float _cpu_history[60];
    float _gpu_history[60];
    float _ram_history[60];
    float _net_dl_history[60];
    float _net_ul_history[60];
    int   _history_idx;
    bool  _first_draw;
    ScreenState _screen;
    unsigned long _lastTouchTime;

    // Time keeping
    unsigned long _lastTimestamp;       // Last Unix timestamp from PC (UTC)
    int           _tzOffset;            // Timezone offset in seconds
    unsigned long _lastTimeSyncMillis;  // millis() when last timestamp received
    unsigned long _disconnectMillis;    // millis() when connection was lost
    bool          _timeValid;           // Have we ever received a timestamp?
    int           _dotAnimState;        // Dot animation frame (0-3)
    unsigned long _lastDotAnim;         // Last dot animation update
};

extern PCMonitorDisplay display;

#endif
