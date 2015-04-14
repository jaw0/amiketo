/*
  Copyright (c) 2013
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2013-Dec-01 00:19 (EST)
  Function: use timer for delay loops

*/

// NB: process must be configured as REALTIME, high priority
//     high priority interrupts (i2c) will cause additional delay


#include <conf.h>
#include <proc.h>
#include <gpio.h>
#include <pwm.h>
#include <stm32.h>
#include <userint.h>

#define TIMER    TIM10
#define CR1_CEN  1
#define CR1_URS  4
#define CR1_OPM  8              // one-shot mode
#define CR1_ARPE (1<<7)         // buffered

int t1, t2;


void
timer_init(utime_t usec){

    RCC->APB2ENR |= 1<<17;	// T10
    nvic_enable(TIM1_UP_TIM10_IRQn, 0x60);

    TIMER->SR &= ~1;
    TIMER->DIER = 1;
    TIMER->CR1  = CR1_URS;

    if( !usec ){
        TIMER->CR1 &= ~CR1_CEN;
        return;
    }

    // the timers run at apb2 freq, unless ...
    int tk = apb2_clock_freq();
    if( tk != sys_clock_freq() ) tk *= 2;
    uint32_t arr = (tk / 1000000) * usec;
    uint32_t psc = 0;

    if( arr & 0xFFFF0000 ){
        // best = find gcd, but it is probably a multiple of 10^n

        if( arr < 0x0FFFFFFF ){
            psc = 1;
            while( arr > 0xFFFF ){
                psc *= 10;
                arr /= 10;
            }
            psc --;
        }else{
            psc = arr >> 16;
            arr /= psc + 1;
        }
    }

    // kprintf("T a: %x, p: %x\n", arr, psc);
    TIMER->PSC = psc;
    TIMER->ARR = arr - 1;

    TIMER->CR1 |= CR1_CEN;
}

void
timer_wait(void){
    tsleep( TIMER, -1, "timer", 0);
}

void
TIM1_UP_TIM10_IRQHandler(void){

    if( !(TIMER->SR & 1) ) return ;

    t1 = SysTick->VAL;
    TIMER->SR  &= ~1;		// clear UIF
    wakeup( TIMER );
}

DEFUN(testtimer, "timer test")
{
    int n = 1000;
    if( argc > 1 ) n = atoi( argv[1] );

    timer_init(n);

    currproc->flags |= PRF_REALTIME;
    currproc->prio = 0;

    timer_wait();
    utime_t t0 = get_hrtime();
    timer_wait();
    utime_t t1 = get_hrtime();

    printf("td %d\n", (int)(t1 - t0));

    return 0;
}

