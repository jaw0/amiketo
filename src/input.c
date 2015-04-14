/*
  Copyright (c) 2014
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2014-Mar-02 23:41 (EST)
  Function: 

*/

#include <conf.h>
#include <proc.h>
#include <msgs.h>
#include <gpio.h>
#include <pwm.h>
#include <ioctl.h>
#include <stm32.h>
#include <userint.h>

#include "board.h"
#include "input.h"
#include "util.h"

extern int ivolume;

int
wait_for_user(void){

    int c = wait_for_action( WAITFOR_BUTTON | WAITFOR_UPDN, -1 );
    switch(c){
    case WAITFOR_BUTTON: return '\n';
    case WAITFOR_UPDN:   return '\e';

    default:
        return 0;
    }
}


/****************************************************************/

#define ZQUIET	100	// acc
#define ZTAP	500	// acc
#define TAPMAXT 600	// msec
#define TQUIET  10	// msec
#define KZ	10


#define set_leds_rgb(a,b)


static inline void
effect_a(void){
    set_leds_rgb( 0x800080, 0x800080 );
    play(ivolume, "A4>>");
}
static inline void
effect_b(void){
    set_leds_rgb( 0x800080, 0x800080 );
    play(ivolume, "D-4>>");
    set_leds_rgb( 0, 0 );
}

void
input_read_all(void){

    read_imu_all();

    // upside-down + button => ^C
    // RSN - upside-down + shake?
    if( accel_z() < -500 && gpio_get(BOT_GPIO_BUTTON) ){
        play(ivolume, "a-4a-4a-4d-3");
        while(1){
            read_imu_all();
            if( accel_z() > 500 ) break;
            usleep(1000);
        }
        play (ivolume, "d-4a-4a-3");
        throw(MSG_CCHAR_0, 1);
    }
}


int
check_button(){

    if( gpio_get( BOT_GPIO_BUTTON ) ){
        effect_a();

        while( gpio_get( BOT_GPIO_BUTTON ) ){
            input_read_all();
            usleep(10000);
        }
        effect_b();
        return 1;
    }
    return 0;
}

int
check_upsidedown(){

    if( accel_z() < 0 ){
        effect_a();
        while(1){
            input_read_all();
            if( accel_z() > 0 ){
                effect_b();
                return 1;
            }
            usleep(10000);
        }
    }
    return 0;
}

int
check_button_or_upsidedown(){
    if( check_button() )     return 1;
    if( check_upsidedown() ) return 1;
    return 0;
}

/****************************************************************/

static void
wait_for_quiet(utime_t limit){
    short  c     = 0;
    int    azave = accel_z() - 1000;
    int    azvar = ABS(azave)/2;

    while(c < 250){
        input_read_all();

        if( check_button() ) break;

        int az = accel_z() - 1000;
        azave = (az + (KZ - 1) * azave) / KZ;
        azvar = (ABS( azave - az ) + (KZ - 1) * azvar) / KZ;

        if( azvar > ZQUIET )
            c = 0;
        else
            c++;

        if( limit && limit <= get_hrtime() ) return;

        usleep(1000);
    }
}

void
wait_for_ready(void){
    // until device is level + still

    short c      = 0;
    int   azave  = accel_z() - 1000;
    int   azvar  = ABS(azave)/2;

    beep_set(100, 1);
    set_leds_rgb( 0x400000, 0x400000 );

    while(c < 1000){
        const char *xx = 0;

        c++;
        input_read_all();
        if( check_button() ) break;

        int az = accel_z() - 1000;
        azave = (az + (KZ - 1) * azave) / KZ;
        azvar = (ABS( azave - az ) + (KZ - 1) * azvar) / KZ;

        if( accel_z() < 500 ) xx = "z<";
        if( ABS(az) > 200  )  xx = "z>";
        if (azvar > ZQUIET  ) xx = "zv";
        if( ABS(accel_x()) > 200 ) xx = "ax";
        if( ABS(accel_y()) > 200 ) xx = "ay";

        if( xx ){
            c = 0;
            // printf("z %d zv %d x %d y %d\n", az, azvar, accel_x(), accel_y());
            printf("=> %s\n", xx);
        }

        usleep(1000);
    }

    set_leds_rgb( 0, 0 );
    beep_set(260, 0);
}

// wait for user to do something (mask + microsec)

// -1 => wait forever
//  0 => don't wait, don't re-read sensors/imu (use existing values)
//
// returns
//   0 got nothing (timeout)
//   N bit that we got

int
wait_for_action(int mask, int timo){

    int8_t i, n=0, q=TQUIET;
    int8_t wn = (mask & WAITFOR_TTAP) ? 3 : 2;
    utime_t t=0;
    utime_t limit = (timo != -1) ? get_hrtime() + timo : 0;

    if( timo ){
        set_leds_rgb( 0x400000, 0x400000 );

        if( mask & (WAITFOR_DTAP | WAITFOR_TTAP) )
            wait_for_quiet(limit);

        set_leds_rgb( 0x40, 0x40 );

        input_read_all();
    }

    while( n != wn ){
        int z = accel_z() - 1000;

        // tap?
        if( (mask & (WAITFOR_DTAP | WAITFOR_TTAP) ) && (ABS(z) > ZTAP) && (q >= TQUIET) ){
            t = get_hrtime();
            q = 0;

            if( n == wn - 1 ){
                effect_b();
            }else{
                effect_a();
                set_leds_rgb( 0x800000, 0x800000 );
            }

            n ++;

        }else{
            if( (ABS(z) < ZQUIET) && (q <= TQUIET) ) q++;

            if( t && t + TAPMAXT * 1000 < get_hrtime() ){
                set_leds_rgb( 0x40, 0x40 );
                n = 0;
                t = 0;
            }
        }

        // upside down
        if( (mask & WAITFOR_UPDN) && check_upsidedown() ){
            return WAITFOR_UPDN;
        }

        // button?
        if( (mask & WAITFOR_BUTTON) && check_button() ){
            return WAITFOR_BUTTON;
        }

        if( limit && limit <= get_hrtime() ){
            set_leds_rgb( 0, 0 );
            return 0;
        }

        usleep(1000);
        input_read_all();
    }

    set_leds_rgb( 0x400040, 0x400040 );
    wait_for_quiet(0);
    set_leds_rgb( 0, 0 );

    return (n == 3) ? WAITFOR_TTAP : WAITFOR_DTAP;
}


#ifdef KTESTING
DEFUN(testbutton, "test button, etc")
{
    ui_pause();
    while(1){
        wait_for_action( WAITFOR_BUTTON | WAITFOR_UPDN, -1 );
    }
    ui_resume();
    return 0;
}


DEFUN(testtap, "test 2tap")
{
    ui_pause();
    while(1){
        wait_for_action( WAITFOR_DTAP, -1 );
    }
    ui_resume();
    return 0;
}

DEFUN(testttap, "test 3tap")
{
    ui_pause();
    while(1){
        wait_for_action( WAITFOR_TTAP, -1 );
    }
    ui_resume();
    return 0;
}

DEFUN(testready, "test ready")
{

    ui_pause();
    while(1){
        wait_for_ready();
        play(8, "abz");
    }
    ui_resume();
    return 0;


}

#endif

