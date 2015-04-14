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


// board_init  - always runs
// board_init2 - skipped on quick wake-and-back-asleep

void
board_init(void){

    bootmsg("board hw init\n");

    // enable i+d cache, prefecth=off => faster + lower adc noise
    // nb: prefetch=on => more faster, less power, more noise
    FLASH->ACR  |= 0x600;

    // beeper
    gpio_init( BOT_GPIO_AUDIO, GPIO_AF(2) | GPIO_SPEED_25MHZ );
    pwm_init(  BOT_TIMER_AUDIO, 440, 255 );
    pwm_set(   BOT_TIMER_AUDIO, 0);

    // LEDs
    gpio_init( BOT_GPIO_LED_WHITE,   GPIO_AF(2) | GPIO_SPEED_25MHZ );
    pwm_set(   BOT_TIMER_LED_WHITE,  0);

    gpio_init( BOT_GPIO_BUTTON,      GPIO_INPUT );
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
    // RSN - turn off power to display, sd
}


void
debug_set_led(int v){
    static initp = 0;

    if( !initp ){
        gpio_init( BOT_GPIO_LED_WHITE,  GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_SPEED_25MHZ );
        initp = 1;
    }

    if( v )
        gpio_set( BOT_GPIO_LED_WHITE );
    else
        gpio_clear( BOT_GPIO_LED_WHITE );

}

