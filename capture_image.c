#include "address_map_arm.h"
#include <stdio.h>  // For basic I/O if needed

#define KEY_BASE              0xFF200050
#define VIDEO_IN_BASE         0xFF203060
#define FPGA_ONCHIP_BASE      0xC8000000

// Simple 5x7 digit patterns (1 = pixel on, 0 = pixel off)
const char font_5x7[10][7] = {
    {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F}, // 0
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // 1
    {0x1F, 0x01, 0x01, 0x1F, 0x10, 0x10, 0x1F}, // 2
    {0x1F, 0x01, 0x01, 0x1F, 0x01, 0x01, 0x1F}, // 3
    {0x11, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01}, // 4
    {0x1F, 0x10, 0x10, 0x1F, 0x01, 0x01, 0x1F}, // 5
    {0x1F, 0x10, 0x10, 0x1F, 0x11, 0x11, 0x1F}, // 6
    {0x1F, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, // 7
    {0x1F, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x1F}, // 8
    {0x1F, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x1F}  // 9
};

// Global variables
int x, y;
short temp_buffer[320];  


void draw_digit(volatile short *mem, int x_pos, int y_pos, int digit, short color) {
    for (y = 0; y < 7; y++) {
        for (x = 0; x < 5; x++) {
            if (font_5x7[digit][y] & (1 << (4 - x))) {
                *(mem + ((y_pos + y) << 9) + (x_pos + x)) = color;
            }
        }
    }
}

void draw_number(volatile short *mem, int x_pos, int y_pos, int number, short color) {
    int digits[4] = {0, 0, 0, 0};
    int i = 3;
    do {
        digits[i--] = number % 10;
        number /= 10;
    } while (number > 0 && i >= 0);

    for (i = 0; i < 4; i++) {
        draw_digit(mem, x_pos + (i * 6), y_pos, digits[i], color);
    }
}

int main(void) {
    volatile int * KEY_ptr          = (int *) KEY_BASE;
    volatile int * Video_In_DMA_ptr = (int *) VIDEO_IN_BASE;
    volatile short * Video_Mem_ptr  = (short *) FPGA_ONCHIP_BASE;

    int photo_count = 0;   
    short text_color = 0x0000;  // black 
    int i;

    int timestamp[6] = {0, 3, 1, 0, 2, 5}; // MMDDYY

    *(Video_In_DMA_ptr + 3) = 0x4;  

    while (1) {
        // Check for KEY0 (bit 0 = 0x1) to capture
        if (*KEY_ptr & 0x1) {
            *(Video_In_DMA_ptr + 3) = 0x0;  // Disable 
            while (*KEY_ptr & 0x1);         
            
            photo_count++;
            if (photo_count > 9999) photo_count = 0;

            
            for (i = 0; i < 6; i++) {
                draw_digit(Video_Mem_ptr, i * 6, 0, timestamp[i], text_color);
            }

            //photo counter below timestamp 
            draw_number(Video_Mem_ptr, 0, 8, photo_count, text_color);

            
            while (1) {
                // Button 1 flip
                if (*KEY_ptr & 0x2) {
                    while (*KEY_ptr & 0x2);  // Wait for KEY1 release
                    for (y = 0; y < 240; y++) {
                      
                        for (x = 0; x < 320; x++) {
                            temp_buffer[x] = *(Video_Mem_ptr + (y << 9) + x);
                        }
                        
                        for (x = 0; x < 320; x++) {
                            *(Video_Mem_ptr + (y << 9) + x) = temp_buffer[319 - x];
                        }
                    }
                }
                // Button 2 greyscale
                else if (*KEY_ptr & 0x4) {
                    while (*KEY_ptr & 0x4); 
                    for (y = 0; y < 240; y++) {
                        for (x = 0; x < 320; x++) {
                            short pixel = *(Video_Mem_ptr + (y << 9) + x);
                            
                            
                            unsigned char r = (pixel >> 11) & 0x1F;    // 5 bits red
                            unsigned char g = (pixel >> 5) & 0x3F;     // 6 bits green
                            unsigned char b = pixel & 0x1F;            // 5 bits blue

                           
                            r = (r << 3) | (r >> 2);  // 5 bits to 8 bits
                            g = (g << 2) | (g >> 4);  // 6 bits to 8 bits
                            b = (b << 3) | (b >> 2);  // 5 bits to 8 bits

                           
                            unsigned char gray = (r + g + b) / 3;

                           
                            short gray_pixel = ((gray >> 3) << 11) |  // 5-bit red
                                             ((gray >> 2) << 5) |    // 6-bit green
                                             (gray >> 3);            // 5-bit blue

                            *(Video_Mem_ptr + (y << 9) + x) = gray_pixel;
                        }
                    }
                }
                // Button 3 invert
                else if (*KEY_ptr & 0x8) {
                    while (*KEY_ptr & 0x8);  // Wait for KEY3 release
                    for (y = 0; y < 240; y++) {
                        for (x = 0; x < 320; x++) {
                            short temp2 = *(Video_Mem_ptr + (y << 9) + x);
                            *(Video_Mem_ptr + (y << 9) + x) = 0xFFFF - temp2; 
                        }
                    }
                    break; 
                }
            }

            // Re-enable video for next capture
            //*(Video_In_DMA_ptr + 3) = 0x4;		//ask if need renable, invert then immediatly enables
        }
    }

    return 0;
}