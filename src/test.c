/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-02 21:36 (EDT)
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
#include <font.h>
#include <userint.h>

#include "board.h"
#include "util.h"


extern void blinky(void);


void
serial_baudtest(void){
    int i;

    while(1){
        for(i=19200; i<76800; i+=100){
            serial_setbaud(0, i);
            printf("\n%d %d %d %d\n", i,i,i,i);
            usleep(100000);
        }
    }
}

void
vcp_shell(void){
    FILE *f = fopen("dev:vcp0", "w");
    if( !f ) return;

    STDIN  = f;
    STDOUT = f;
    STDERR = f;
    shell();
}

void
xxxmain(void){

    //debug_set_led(255);
    //while(1){
    //    debug_set_led(255);
    //    printf("a");
    //    play(8, "a");
    //    debug_set_led(0);
    //    play(8, "b");
    //}


    board_init();
    play(16, "g4g4g4f4z4a4g2");
    start_proc(1024, blinky, "blinky");
    // start_proc(2048, vcp_shell, "shell2");

    //while(1){
    //    play(8, "a3 z3z3z3");
    //    usleep(500000);
    //}
}


extern const struct Font * const fonts[N_FONT];

DEFUN(fonttest, "font test")
{
    int i;
    ui_pause();
#if 1
    for(i=0; i<N_FONT; i++){
        set_font( fonts[i]->name );
        printf("%s\nAfgiyO0\n", fonts[i]->name);
        sleep(1);
    }
    sleep(2);
#endif
#if 0
    set_font("4x6");
    printf("4x6\n");
    set_font("10x20");
    printf("10x20\n");
    sleep(1);
#endif

    set_font("profont12");
    printf("~!@#$%^&*()_+_=\n");
    printf("0123456789:;,.?\n");
    printf("abcfghpqrtijABC\n");
    sleep(5);
    
    ui_resume();
    return 0;
}
