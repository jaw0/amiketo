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


// hwinit      - called early in init process, to init hw
// board_init  - always runs
// board_init2 - skipped on quick wake-and-back-asleep

void
hwinit(void){

#if defined(AMIKETO_v1) || defined(AMIKETO_v2)
    // enable power to sdcard+display (not on v0 board)
    // must be enabled before we init the card or dpy
    gpio_init( HWCF_GPIO_NPWREN, GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_SPEED_25MHZ );
    gpio_clear( HWCF_GPIO_NPWREN );
#endif
#ifdef AMIKETO_v2
    // and toggle nrst2
    gpio_init( HWCF_GPIO_NRST2, GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_SPEED_25MHZ );
    gpio_clear( HWCF_GPIO_NRST2 );
    for(i=0; i<200000; i++){ asm("nop"); }
    gpio_set( HWCF_GPIO_NRST2 );
#endif
}

void
board_init(void){

    bootmsg("board hw init\n");

    // enable i+d cache, prefetch=off => faster + lower adc noise
    // nb: prefetch=on => more faster, less power, more noise
    FLASH->ACR  |= 0x600;

    // beeper
    gpio_init( HWCF_GPIO_AUDIO, GPIO_AF(2) | GPIO_SPEED_25MHZ );
    pwm_init(  HWCF_TIMER_AUDIO, 440, 255 );
    pwm_set(   HWCF_TIMER_AUDIO, 0);

    // LED
    gpio_init( HWCF_GPIO_LED_WHITE,   GPIO_AF(2) | GPIO_SPEED_25MHZ );
    pwm_set(   HWCF_TIMER_LED_WHITE,  0);

    // button
    gpio_init( HWCF_GPIO_BUTTON,      GPIO_INPUT );

}

void
board_init2(void){

    imu_init();
    pmic_init();
}

void
board_disable(void){

    imu_disable();
    pmic_disable();

#if defined(AMIKETO_v1) || defined(AMIKETO_v2)
    gpio_set( HWCF_GPIO_NPWREN );
#endif
}


void
debug_set_led(int v){
    static initp = 0;

    if( !initp ){
        gpio_init( HWCF_GPIO_LED_WHITE,  GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_SPEED_25MHZ );
        initp = 1;
    }

    if( v )
        gpio_set( HWCF_GPIO_LED_WHITE );
    else
        gpio_clear( HWCF_GPIO_LED_WHITE );

}

