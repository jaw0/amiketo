/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-09 23:10 (EDT)
  Function: startup screen

*/

extern "C" {
#include <conf.h>
#include <proc.h>
#include <ioctl.h>
#include <error.h>
#include <stm32.h>
};

#include "gfxdpy.h"

extern "C" void splash( GFXdpy* );
extern "C" int was_rtc_wakeup(void);

static uint8_t image[] = {

// compressed 128x64 startup screen
255, 255, 255, 255, 255, 255, 255, 114, 8, 255, 113, 9, 255, 111, 1,
113, 11, 125, 1, 255, 8, 2, 19, 3, 28, 2, 36, 2, 41, 1, 49, 2, 61, 6,
70, 7, 79, 4, 110, 6, 121, 6, 255, 9, 2, 20, 3, 27, 4, 36, 2, 41, 2,
48, 3, 59, 4, 64, 3, 70, 11, 89, 8, 108, 9, 122, 6, 255, 8, 1, 10, 2,
20, 3, 27, 4, 36, 2, 41, 2, 48, 3, 57, 3, 71, 2, 75, 2, 87, 6, 94, 4,
107, 10, 123, 5, 255, 7, 2, 10, 2, 20, 4, 26, 2, 29, 2, 36, 2, 41, 2,
47, 2, 56, 3, 75, 2, 86, 2, 89, 1, 96, 3, 106, 5, 114, 3, 126, 2, 255,
7, 1, 11, 2, 19, 2, 22, 2, 26, 2, 29, 2, 36, 2, 41, 2, 45, 2, 55, 3,
64, 1, 75, 2, 85, 2, 96, 3, 105, 5, 114, 3, 127, 1, 255, 3, 10, 19, 2,
23, 2, 26, 1, 29, 2, 35, 2, 41, 2, 44, 3, 54, 12, 75, 2, 84, 3, 96, 2,
105, 3, 255, 5, 2, 12, 2, 19, 1, 23, 3, 29, 2, 35, 2, 41, 7, 54, 3,
75, 2, 84, 2, 96, 2, 104, 3, 255, 4, 2, 12, 3, 18, 2, 24, 2, 29, 3,
35, 2, 41, 2, 45, 4, 54, 3, 67, 2, 75, 3, 84, 2, 95, 2, 103, 4, 255,
3, 2, 13, 2, 18, 2, 24, 1, 30, 2, 34, 2, 41, 2, 47, 4, 54, 5, 63, 5,
75, 7, 84, 3, 93, 2, 103, 3, 255, 1, 3, 17, 2, 33, 2, 42, 1, 49, 3,
55, 10, 75, 5, 85, 8, 102, 3, 255, 102, 3, 255, 101, 4, 255, 101, 3,
255, 101, 3, 255, 101, 3, 255, 72, 2, 89, 2, 101, 3, 255, 72, 2, 89,
2, 101, 3, 255, 72, 2, 89, 2, 101, 3, 255, 66, 16, 89, 2, 101, 3, 123,
5, 255, 66, 15, 89, 2, 101, 3, 122, 6, 255, 71, 2, 89, 2, 101, 3, 110,
3, 121, 7, 255, 70, 9, 88, 4, 101, 3, 109, 5, 120, 4, 127, 1, 255, 70,
9, 88, 4, 101, 3, 108, 7, 120, 3, 255, 69, 2, 72, 2, 77, 2, 87, 2, 90,
3, 101, 3, 107, 4, 112, 4, 120, 3, 255, 69, 2, 72, 3, 76, 2, 87, 2,
91, 2, 101, 3, 107, 3, 113, 3, 120, 3, 255, 68, 2, 73, 4, 86, 2, 91,
3, 101, 3, 107, 4, 112, 4, 120, 4, 127, 1, 255, 67, 2, 73, 3, 86, 2,
92, 2, 101, 3, 108, 7, 121, 7, 255, 66, 2, 73, 4, 85, 2, 92, 3, 101,
3, 109, 5, 122, 6, 255, 66, 1, 71, 3, 75, 3, 84, 2, 93, 3, 101, 3,
110, 3, 123, 5, 255, 69, 4, 76, 4, 83, 2, 94, 3, 101, 3, 255, 67, 4,
77, 7, 95, 3, 101, 3, 255, 67, 2, 79, 2, 82, 1, 96, 1, 101, 3, 255,
101, 3, 255, 44, 15, 101, 27, 255, 12, 3, 44, 15, 101, 27, 255, 12, 3,
29, 3, 35, 3, 44, 2, 47, 6, 56, 3, 102, 26, 255, 6, 2, 12, 3, 19, 2,
28, 1, 32, 1, 34, 1, 38, 1, 44, 2, 47, 5, 53, 3, 57, 2, 114, 3, 123,
3, 255, 5, 4, 10, 7, 18, 4, 28, 1, 32, 1, 34, 1, 44, 2, 47, 5, 53, 3,
57, 2, 114, 3, 123, 3, 255, 5, 17, 28, 1, 32, 1, 35, 2, 44, 2, 47, 5,
53, 3, 57, 2, 114, 3, 123, 3, 255, 6, 15, 28, 1, 32, 1, 37, 1, 44, 2,
47, 5, 53, 3, 57, 2, 114, 3, 123, 3, 255, 7, 13, 28, 1, 32, 1, 38, 1,
44, 2, 47, 5, 53, 3, 57, 2, 114, 3, 123, 3, 255, 6, 15, 28, 1, 32, 1,
34, 1, 38, 1, 44, 2, 47, 5, 53, 3, 57, 2, 114, 3, 123, 3, 255, 6, 6,
15, 6, 29, 3, 35, 3, 44, 2, 51, 2, 56, 3, 114, 3, 123, 3, 255, 3, 8,
16, 8, 44, 15, 255, 3, 8, 16, 8, 44, 15, 255, 3, 8, 16, 8, 31, 1, 34,
5, 44, 3, 50, 3, 56, 3, 65, 1, 70, 1, 74, 1, 78, 1, 255, 6, 6, 15, 6,
31, 1, 34, 1, 44, 2, 47, 3, 51, 1, 53, 3, 57, 2, 65, 1, 69, 1, 71, 1,
74, 1, 78, 1, 255, 6, 7, 14, 7, 31, 1, 34, 4, 44, 2, 47, 5, 53, 3, 57,
2, 65, 1, 68, 1, 72, 1, 74, 1, 76, 1, 78, 1, 255, 7, 5, 15, 5, 31, 1,
34, 1, 38, 1, 44, 2, 47, 5, 53, 3, 57, 2, 65, 1, 68, 1, 72, 1, 74, 1,
76, 1, 78, 1, 255, 6, 6, 15, 6, 31, 1, 38, 1, 44, 2, 47, 2, 51, 1, 53,
3, 57, 2, 65, 1, 68, 5, 74, 1, 76, 1, 78, 1, 255, 5, 6, 16, 6, 31, 1,
38, 1, 44, 2, 47, 3, 51, 1, 53, 3, 57, 2, 65, 1, 68, 1, 72, 1, 74, 1,
76, 1, 78, 1, 255, 5, 4, 10, 1, 16, 1, 18, 4, 28, 1, 31, 1, 34, 1, 38,
1, 44, 2, 47, 3, 51, 1, 53, 3, 57, 2, 62, 1, 65, 1, 68, 1, 72, 1, 74,
2, 77, 2, 255, 6, 2, 19, 2, 29, 2, 35, 3, 44, 3, 50, 3, 56, 3, 63, 2,
68, 1, 72, 1, 75, 1, 77, 1, 255, 44, 15, 255,

};


void
splash( GFXdpy *dpy ){
    short x=0, y=0;
    short i=0, l;

    dpy->clear_screen();
    if( was_rtc_wakeup() ){
        dpy->flush();
        return;
    }
    // RSN - multiple screen sizes? color?

    while( y < 64 && i < sizeof(image) ){
        x = image[i++];
        if( x == 0xFF ){
            x = 0;
            y ++;
            continue;
        }
        l = image[i++];
        for( ; l; l--){
            dpy->set_pixel(x, y, 0xFFFFFF);
            x++;
        }
    }

    dpy->puts("\e[14m");	// 6x10 font

    // RSN - these should come from conf
    dpy->set_pos(6, 30);
    dpy->puts("STM32F411");

    dpy->set_pos(62, 45);
    dpy->puts("Rev. 0");

    dpy->set_pos(80,55);
    dpy->puts("/2015-04");

    dpy->flush();
}

