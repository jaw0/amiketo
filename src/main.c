/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-02 21:37 (EDT)
  Function: 

*/

#include <conf.h>
#include <proc.h>
#include <gpio.h>
#include <pwm.h>
#include <adc.h>
#include <spi.h>
#include <i2c.h>
#include <ioctl.h>
#include <error.h>
#include <stm32.h>
#include <userint.h>

#include "util.h"


#define FLASHSTART	0x08000000
extern const u_char* _etext;

extern void blinky(void);

// first look on the card, then flash
// NB: if we check the flash first, a bad config could brick the system
#define RUN_SCRIPT(file)        (run_script("sd0:" file) && run_script("fl0:" file))

DEFVAR(int, dpy_ui_enable,    1, UV_TYPE_UL | UV_TYPE_CONFIG, "run user-interface on display")
DEFVAR(int, serial_enable,    1, UV_TYPE_UL | UV_TYPE_CONFIG, "run shell on serial")
DEFVAR(int, logger_autostart, 1, UV_TYPE_UL | UV_TYPE_CONFIG, "start logger automatically at boot")

DEFUN(save, "save all config data")
{
    // save_config("fl0:config.rc");
    save_config("config.rc");
    return 0;
}


// make sure we have 2 copies of the file (on card + on flash)
static void
two_copies(const char *file){
    char bufc[128];
    char buff[128];
    FILE *f1, *f2;

    snprintf(bufc, sizeof(bufc), "sd0:%s", file);
    snprintf(buff, sizeof(bufc), "fl0:%s", file);

    f1 = fopen(buff, "r");
    f2 = fopen(bufc, "r");
    if( f1 ) fclose(f1);
    if( f2 ) fclose(f2);

    if( f1 && f2 )   return;    // all good.
    if( !f1 && !f2 ) return;    // all bad.

    if( f1 ){
        // copy fl -> sd
        f1 = fopen(buff, "r");
        f2 = fopen(bufc, "w");
    }else{
        // copy sd -> fl
        f1 = fopen(bufc, "r");
        f2 = fopen(buff, "w");
    }
    if( !f1 || !f2 ){
        // open failed - probably no card
        if( f1 ) fclose(f1);
        if( f2 ) fclose(f2);
        return;
    }

    int i;
    while( (i=fread(f1, bufc, sizeof(bufc))) > 0 ){
        fwrite(f2, bufc, i);
    }

    fclose(f1);
    fclose(f2);
}

unsigned int
random(void){
    static unsigned int x;
    unsigned int y;

    y = RTC->SSR ^ RTC->TR;
    y ^= RTC->DR;
    y ^= SysTick->VAL;

    x = (x<<7) | (x>>25);
    x ^= y;
    return x;
}


#if defined(USE_SSD1306)
#  define dpy_puts	ssd13060_puts
#elif defined(USE_SSD1331)
#  define dpy_puts	ssd13310_puts
#elif defined(USE_EPAPER)
#  define dpy_puts	epaper0_puts
#endif


// on panic: beep, flash, and display message
// NB: system will also output to the console
void
onpanic(const char *msg){
    int i;

    set_led_white( 0xFF );
    beep_set(200, 127);

    splproc();
    currproc = 0;

    dpy_puts("\e[J\e[16m*** PANIC ***\r\n\e[15m");
    if( msg ){
        dpy_puts(msg);
        dpy_puts("\r\n");
    }
    splhigh();

    while(1){
        set_led_white( 0xFF );
        beep_set(150, 127);
        for(i=0; i<5000000; i++){ asm("nop"); }
        set_led_white( 0x1F );
        beep_set(250, 127);
        for(i=0; i<5000000; i++){ asm("nop"); }
    }
}

void
set_onwake(int val){
    RTC->BKP1R = val;
}

//################################################################

// put all peripherals in their lowest power mode
// put the processor into "standby" mode

void
shutdown(void){
    kprintf("powering down\n");
    ui_sleep();
    board_disable();
    power_down();
}


DEFUN(shutdown, "shut down system")
DEFALIAS(shutdown, poweroff)
DEFALIAS(shutdown, halt)
{
    shutdown();
    return 0;
}


//################################################################

extern const struct Menu guitop;

void
uiproc(void){
    FILE *f = fopen("dev:oled0", "w");
    STDOUT = f;
    STDIN  = 0;

    usleep( 100000 );

    menu( &guitop );
}


void
serproc(void){
    FILE *f = fopen("dev:serial0", "w");
    STDOUT = f;
    STDIN  = f;
    STDERR = f;

    usleep( 100000 );
    shell();
}

//################################################################


// use rtc reg for mode

void
main(void){

    board_init();
    RUN_SCRIPT("config.rc");

    // is this a reset or wakeup?
    // wakeup + logger mode => log once + power down

    if( was_rtc_wakeup() ){
        RUN_SCRIPT("wakeup.rc");

        int onwake = RTC->BKP1R;
        set_onwake( 0 );
        if( onwake > FLASHSTART && onwake < (int)&_etext ){
            void (*func)(void) = (void(*)(void))onwake;
            func();
        }
    }

    set_onwake( 0 );
    set_led_white( 0xFF );	// flash LED
    // two_copies("config.rc");	// to be safe
    board_init2();

    set_led_white( 0 );
    play(6, "g4g4g4f4z4a4g2");
    RUN_SCRIPT("startup.rc");

    start_proc(512, blinky, "blinky");

    if( dpy_ui_enable )
        start_proc(2048, uiproc, "ui");

    if( serial_enable )
        start_proc(2048, serproc, "shell");

    if( logger_autostart )
        logger_init();

    printf("\n");

}

