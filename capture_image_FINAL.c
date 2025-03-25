#include <stdio.h>
#include <time.h>
#include "address_map_arm.h"

#define KEY_BASE              0xFF200050
#define VIDEO_IN_BASE         0xFF203060
#define FPGA_ONCHIP_BASE      0xC8000000

#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240
#define ROW_STRIDE     512  // each row is 512 pixels wide in memory

// Define black color for drawing text (in 5-6-5 format)
#define BLACK_COLOR    0x0000


volatile int picture_counter = 0;

// --- Simple 8x8 font data ---
unsigned char font[128][8] = {
  ['0'] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x66, 0x3C},
  ['1'] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C},
  ['2'] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x66, 0x7E},
  ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x06, 0x66, 0x3C},
  ['4'] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x1E},
  ['5'] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x06, 0x66, 0x3C},
  ['6'] = {0x3C, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x3C},
  ['7'] = {0x7E, 0x66, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18},
  ['8'] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x66, 0x3C},
  ['9'] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x18, 0x38},
  ['P'] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x60},
  ['I'] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C},
  ['C'] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x60, 0x66, 0x3C},
  [':'] = {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00},
  [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

void draw_pixel(volatile short *Video_Mem_ptr, int x, int y, short color) {
    if(x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        *(Video_Mem_ptr + (y * ROW_STRIDE) + x) = color;
    }
}

void draw_char(volatile short *Video_Mem_ptr, int x, int y, char c, short color) {
    int row, col;
    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            // Check if the bit is set; highest-order bit is the leftmost pixel.
            if (font[(int)c][row] & (0x80 >> col)) {
                draw_pixel(Video_Mem_ptr, x + col, y + row, color);
            }
        }
    }
}

void draw_string(volatile short *Video_Mem_ptr, int x, int y, const char *str, short color) {
    while(*str) {
        draw_char(Video_Mem_ptr, x, y, *str, color);
        x += 8;  // move right by 8 pixels for each character
        str++;
    }
}

// --- Get real time timestamp ---
// This function uses the standard C library to obtain the current time.
void get_timestamp(char *buffer) {
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(buffer, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

// --- Overlay timestamp and picture counter on the image ---
void overlay_info(volatile short *Video_Mem_ptr, int picture_counter) {
    char info[32];
    char timestamp[16];
    get_timestamp(timestamp);

    sprintf(info, "PIC:%d T:%s", picture_counter, timestamp);
    // Draw the string at a fixed location (top-left corner) in black.
    draw_string(Video_Mem_ptr, 5, 5, info, BLACK_COLOR);
}

void grayscale_image(volatile short *Video_Mem_ptr) {
    int x, y;
    for (y = 0; y < SCREEN_HEIGHT; y++) {
        for (x = 0; x < SCREEN_WIDTH; x++) {
            short pixel = *(Video_Mem_ptr + (y * ROW_STRIDE) + x);
            int red   = (pixel >> 11) & 0x1F;
            int green = (pixel >> 5)  & 0x3F;
            int blue  = pixel & 0x1F;
            // Convert to 8-bit approximations
            int red8   = (red << 3)   | (red >> 2);
            int green8 = (green << 2) | (green >> 4);
            int blue8  = (blue << 3)  | (blue >> 2);
            int gray8 = (red8 + green8 + blue8) / 3;
            int red5   = gray8 >> 3;
            int green6 = gray8 >> 2;
            int blue5  = gray8 >> 3;
            short gray_pixel = (red5 << 11) | (green6 << 5) | blue5;
            *(Video_Mem_ptr + (y * ROW_STRIDE) + x) = gray_pixel;
        }
    }
}

// --- Mirror horizontally ---
void mirror_image(volatile short *Video_Mem_ptr) {
    int x, y;
    for (y = 0; y < SCREEN_HEIGHT; y++) {
        for (x = 0; x < SCREEN_WIDTH/2; x++) {
            int opp = SCREEN_WIDTH - 1 - x;
            short temp = *(Video_Mem_ptr + (y * ROW_STRIDE) + x);
            *(Video_Mem_ptr + (y * ROW_STRIDE) + x) = *(Video_Mem_ptr + (y * ROW_STRIDE) + opp);
            *(Video_Mem_ptr + (y * ROW_STRIDE) + opp) = temp;
        }
    }
}

// --- Invert the image ---
void invert_image(volatile short *Video_Mem_ptr) {
    int x, y;
    for (y = 0; y < SCREEN_HEIGHT; y++) {
        for (x = 0; x < SCREEN_WIDTH; x++) {
            short pixel = *(Video_Mem_ptr + (y * ROW_STRIDE) + x);
            int red   = (pixel >> 11) & 0x1F;
            int green = (pixel >> 5)  & 0x3F;
            int blue  = pixel & 0x1F;
            red = 31 - red;
            green = 63 - green;
            blue = 31 - blue;
            short inverted = (red << 11) | (green << 5) | blue;
            *(Video_Mem_ptr + (y * ROW_STRIDE) + x) = inverted;
        }
    }
}

int main(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();

    volatile int * KEY_ptr = (int *) KEY_BASE;
    volatile int * Video_In_DMA_ptr = (int *) VIDEO_IN_BASE;
    volatile short * Video_Mem_ptr = (short *) FPGA_ONCHIP_BASE;
    int keys;
   
    // Start video
    *(Video_In_DMA_ptr + 3) = 0x4;
   
    while (1) {
        // --- Capture picture ---
        while (!(*KEY_ptr & 0x1));  // wait for KEY0 press
       
        // Freeze video capture 
        *(Video_In_DMA_ptr + 3) = 0x0;
        // Debounce
        while (*KEY_ptr & 0x1);
       
        picture_counter++;
        
        overlay_info(Video_Mem_ptr, picture_counter);
       
        // KEY1: Grayscale, KEY2: Mirror, KEY3: Invert.
        while (1) {
            keys = *KEY_ptr;
            if (keys & 0x2) {  // KEY1 pressed: Grayscale conversion
                grayscale_image(Video_Mem_ptr);
                while (*KEY_ptr & 0x2);  // wait for key release
            }
            if (keys & 0x4) {  // KEY2 pressed: Mirror image
                mirror_image(Video_Mem_ptr);
                while (*KEY_ptr & 0x4);
            }
            if (keys & 0x8) {  // KEY3 pressed: Invert image
                invert_image(Video_Mem_ptr);
                while (*KEY_ptr & 0x8);
            }
            // If KEY is pressed again, break out to capture a new image.
            if (*KEY_ptr & 0x1) {
                // Re-enable video stream to prepare for the next capture.
                *(Video_In_DMA_ptr + 3) = 0x4;
                while (*KEY_ptr & 0x1);
                break;
            }
        }
    }
   
    return 0;
}
