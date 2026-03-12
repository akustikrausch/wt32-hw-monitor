#ifndef CONFIG_H
#define CONFIG_H

// === WT32-SC01 Pin Definitions ===
// SPI (ST7796S Display)
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC    21
#define TFT_RST   22
#define TFT_BL    23

// I2C (FT5x06 Touch)
#define TOUCH_SDA 18
#define TOUCH_SCL 19

// === Display ===
#define SCREEN_W  480
#define SCREEN_H  320

// === Serial Protocol ===
#define SERIAL_BAUD  115200

// === History for graphs ===
#define HISTORY_LEN  60   // 60 samples = 30 seconds at 2Hz

// === Colors (RGB565) ===
#define COL_BG        0x0000  // Black
#define COL_HEADER    0x1082  // Dark gray
#define COL_TEXT      0xFFFF  // White
#define COL_LABEL     0xAD55  // Light gray
#define COL_GREEN     0x07E0  // Green
#define COL_YELLOW    0xFFE0  // Yellow
#define COL_RED       0xF800  // Red
#define COL_BLUE      0x001F  // Blue
#define COL_CYAN      0x07FF  // Cyan
#define COL_ORANGE    0xFD20  // Orange
#define COL_BAR_BG    0x2104  // Dark gray for bar background
#define COL_DIVIDER   0x4208  // Medium gray

// === Layout (Y positions) — no header, start at top ===
#define CPU_GPU_Y     2
#define CPU_GPU_H     150
#define RAM_Y         (CPU_GPU_Y + CPU_GPU_H + 2)
#define RAM_H         26
#define STORAGE_Y     (RAM_Y + RAM_H + 2)
#define STORAGE_H     26
#define BOTTOM_Y      (STORAGE_Y + STORAGE_H + 2)
#define BOTTOM_H      50  // Fans + disk temps

// CPU/GPU columns
#define COL_LEFT_X    4
#define COL_LEFT_W    234
#define COL_RIGHT_X   242
#define COL_RIGHT_W   234

// Bar dimensions
#define BAR_H         14

// Graph dimensions
#define GRAPH_W       220
#define GRAPH_H       60

// === Temperature thresholds ===
#define TEMP_WARN     60
#define TEMP_CRIT     80

#endif
