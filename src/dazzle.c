/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-02 21:36 (EDT)
  Function: lights and sounds

*/

#include <conf.h>
#include <proc.h>
#include <gpio.h>
#include <pwm.h>
#include <adc.h>
#include <spi.h>
#include <i2c.h>
#include <ioctl.h>
#include <stm32.h>
#include <userint.h>

#include "board.h"

int volume_setting = 4;
int volume  = 16;       // music
int ivolume = 16;       // UI

int blinky_override = 0;

void
set_volume(int v){
    if( v > 7 ) v = 7;
    volume_setting = v;
    ivolume = volume = 1<<v;
    if( ivolume < 2 ) ivolume = 2;
}


DEFUN(volume, "set volume")
{
    if( argc == 2 ){
        set_volume( atoi(argv[1]) );
    }else if( argv[0][0] == '-' ){
        int v = menu_get_int("Volume", 0, 0, 7, volume_setting);
        if( v != -1 ) set_volume( v );
    }else{
        printf("volume: %d\n", volume_setting);
    }

    return 0;
}

void
beep_set(int freq, int vol){
    if( vol > 128 ) vol = 128;
    if( vol < 0  )  vol = 0;

    freq_set(BOT_TIMER_AUDIO, freq);
    pwm_set(BOT_TIMER_AUDIO,  vol);
}

void
beep(int freq, int vol, int dur){
    if( vol > 128 ) vol = 128;
    if( vol < 1  )  vol = 1;

    freq_set(BOT_TIMER_AUDIO, freq);
    pwm_set(BOT_TIMER_AUDIO,  vol);
    usleep(dur);
    pwm_set(BOT_TIMER_AUDIO,  0);
}

DEFUN(beep, "beep")
{

    if( argc > 2 )
        beep( atoi(argv[1]), atoi(argv[2]), 250000 );
    else
        fprintf(STDERR, "beep freq volume\n");

    return 0;
}

void
set_led_white(int v){
    pwm_set( BOT_TIMER_LED_WHITE, v & 255 );
}

void
printadj(const char *label, const char *msg){
    int l = strlen(msg);
    int s;

    if( l < 13 ) s = 17;
    else if( l < 15 ) s = 16;
    else if( l < 22 ) s = 15;
    else s = 13;

    printf("\e[J\e[15mM5/471 %s\n\e[%dm%s\n", label, s, msg);

}


/****************************************************************/
static u_char _blinky_pattern[][40] = {
    {
        1, 2, 4, 8, 16,
        16, 8, 4, 2, 1,
        0, 0, 0, 0,
        1, 2, 4, 8, 16,
        16, 8, 4, 2, 1,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
    {
        1, 2, 4, 8, 16,
        16, 8, 4, 2, 1,
        0, 0, 0,
        1, 2, 4, 8, 16,
        16, 8, 4, 2, 1,
        0, 0, 0,
        1, 2, 4, 8, 16,
        16, 8, 4, 2, 1,
        0, 0, 0, 0,
    },
    {
        1, 4, 16, 4, 1,
        1, 4, 16, 4, 1,
        1, 4, 16, 4, 1,
        1, 4, 16, 4, 1,
        1, 4, 16, 4, 1,
        1, 4, 16, 4, 1,
        1, 4, 16, 4, 1,
        1, 4, 16, 4, 1,
    },
    {
        // candle flicker
        75, 0, 87, 64, 197, 48, 176, 253, 134, 63, 243, 142, 83,
        71, 10, 0, 71, 101, 117, 132, 94, 108, 46, 0, 214, 54, 100,
        90, 0, 232, 63, 44, 99, 62, 148, 224, 111, 123, 249, 136
    },
    {
        // logger overrun
        0, 0, 0, 32, 32,
        0, 0, 0, 32, 32,
        0, 0, 0, 32, 32,
        0, 0, 0, 32, 32,
        0, 0, 0, 32, 32,
        0, 0, 0, 32, 32,
        0, 0, 0, 32, 32,
        0, 0, 0, 32, 32,

    },
};
static int current_blink_pattern = 0;

void
set_blinky(int p){
    current_blink_pattern = p;
}

DEFUN(set_blinky, "set blink pattern")
{
    if( argc > 1 )
        current_blink_pattern = atoi(argv[1]);
    return 0;
}

void
update_blinky(void){
    static int i = 0;

    if( blinky_override ) return;

    if( ++i >= 40 ) i = 0;

    set_led_white( _blinky_pattern[current_blink_pattern][i] );
}

void
blinky(void){
    while(1){
        update_blinky();
        usleep(25000);
    }
}

