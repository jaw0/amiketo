/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-04 00:13 (EDT)
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

DEFUN(clock, "display clock")
{
    struct tm t;

    while(1){
        read_imu_quick();
        utime_t now = get_time();
        gmtime_r( &now, &t );

        set_font("ncenB18_n");
        printf("\e[16;0=");
        printf("%02.2d:%02.2d", t.tm_hour, t.tm_min);
        set_font("ncenB10_n");
        printf(" %02.2d\n", t.tm_sec);

        set_font("ncenB10_n");
        printf("\e[-1;0H");
        printf("%04.4d-%02.2d-%02.2d\n", t.tm_year, t.tm_mon, t.tm_mday);

        //printf("\e[J\e[15m\e[2s");	// 6x12; scaled*2
        //printf("\n %02.2d:%02.2d:%02.2d\n", t.tm_hour, t.tm_min, t.tm_sec);

        if( check_button_or_upsidedown() ) break;
        usleep( 1000000 - now % 1000000 );
    }
    printf("\e[0s");
    return 0;
}
