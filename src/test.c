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
#include <userint.h>

#include "board.h"


extern void blinky(void);


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

