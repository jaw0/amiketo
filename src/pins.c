/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-05 10:05 (EDT)
  Function: configure pins, etc

*/

#include <conf.h>
#include <proc.h>
#include <stm32.h>
#include <nvic.h>
#include <gpio.h>
#include <adc.h>
#include <pwm.h>
#include <userint.h>

#include "util.h"

#define USE_ADCDMA

#define PINMODE_NONE	0
#define PINMODE_ADC	1
#define PINMODE_IN	2
#define PINMODE_OUT	3
#define PINMODE_PWM	4
#define PINMODE_DAC	5
#define PINMODE_TOUCH	6

#define NUM_PINS	10


#ifdef USE_ADCDMA
static short dmabuf[16];
static int   dmasqr[3];
static int   adc_count;
static int   adc_acqmask;
static void adc_dma_sqr_init(int);
static void adc_dma_get_all(void);
#endif


struct pin_descr {
    const char *name;
    short       gpio;
    short       adc;
    short       timer;
    short	timeraf;
    short       dac;	// coming soon...
    short	touch;	// coming soon...
};
const struct pin_descr descr[] = {
    { "A0", GPIO_A0, ADC_1_0, TIMER_2_1, 1, -1, -1 },
    { "A1", GPIO_A1, ADC_1_1, TIMER_2_2, 1, -1, -1 },
    { "A2", GPIO_A2, ADC_1_2, TIMER_2_3, 1, -1, -1 },
    { "A3", GPIO_A3, ADC_1_3, TIMER_2_4, 1, -1, -1 },
    { "A4", GPIO_A4, ADC_1_4, -1,        0,  1, -1 },
    { "A5", GPIO_A5, ADC_1_5, -1,        0,  2, -1 }, // T8/1N
    { "A6", GPIO_A6, ADC_1_6, TIMER_3_1, 2, -1, -1 },
    { "A7", GPIO_A7, ADC_1_7, TIMER_3_2, 2, -1, -1 },
    { "B0", GPIO_B0, ADC_1_8, TIMER_3_3, 2, -1, -1 },
    { "B1", GPIO_B1, ADC_1_9, TIMER_4_4, 2, -1, -1 },

    // if the serial port is disabled
#ifndef USE_SERIAL
    { "A9",  GPIO_A9,  -1,      TIMER_1_2, 1, -1, -1 },
    { "A10", GPIO_A10, -1,      TIMER_1_3, 1, -1, -1 },
#endif
};


struct pin_config {
    short	mode;
    short	gpcf;
    short	adcsamp;
    short	pwmfreq;
    short	pwmmax;
};

struct pin_config config[NUM_PINS];

static const short sampletime[] = {
    3, 15, 28, 56, 84, 112, 144, 480
};

int
find_pin(const char *pin){
    short i;

    for(i=0; i<ELEMENTSIN(descr); i++){
        if( !strcasecmp(pin, descr[i].name) ) return i;
    }
    return -1;
}

int
best_sample_value(int samp){
    short i;

    for(i=0; i<ELEMENTSIN(sampletime); i++){
        if( sampletime[i] >= samp ) return i;
    }

    return ELEMENTSIN(sampletime) - 1;
}


// pinmode PIN MODE [...]
// pinmode A0 adc [samples 20]
// pinmode A0 pwm [freq 10000] [max 255]
// pinmode A0 input [pullup|pulldown]
// pinmode A0 output [pushpull|opendrain|pullup]

DEFUN(pinmode, "set pin mode")
{
    if( argc < 3 ){
        fprintf(STDERR, "error: pinmode pin mode [...]\n");
        return 0;
    }

    short p = find_pin(argv[1]);
    if( p == -1 ){
        fprintf(STDERR, "invalid pin '%s'\n", argv[1]);
        return 0;
    }

    if( !strcmp(argv[2], "none") ){
        gpio_init( descr[p].gpio, GPIO_INPUT );
        config[p].mode = PINMODE_NONE;
    }

    else if( !strcmp(argv[2], "input") ){
        short m = GPIO_INPUT;
        if( argc > 3 ){
            if( !strcmp(argv[3], "pullup") )   m |= GPIO_PULL_UP;
            if( !strcmp(argv[3], "pulldown") ) m |= GPIO_PULL_DN;
        }
        gpio_init( descr[p].gpio,  m );
        config[p].mode = PINMODE_IN;
        config[p].gpcf = m;
    }

    else if( !strcmp(argv[2], "output") ){
        short m = GPIO_OUTPUT | GPIO_SPEED_25MHZ;
        if( argc > 3 ){
            if( !strcmp(argv[3], "pushpull") )  m |= GPIO_PUSH_PULL;
            if( !strcmp(argv[3], "opendrain") ) m |= GPIO_OPEN_DRAIN;
            if( !strcmp(argv[3], "pullup") )    m |= GPIO_OPEN_DRAIN | GPIO_PULL_UP;
        }
        gpio_init( descr[p].gpio,  m );
        config[p].mode = PINMODE_OUT;
        config[p].gpcf = m;
    }

    else if( !strcmp(argv[2], "adc") ){
        if( descr[p].adc == -1 ){
            fprintf(STDERR, "adc not supported on pin %s\n", argv[1]);
            return 0;
        }
        short m = GPIO_ANALOG;
        short s = 1;

        // QQQ - or specify the circuit impedance?
        if( argc > 4 && !strcmp(argv[3], "samples") ) s = best_sample_value( atoi( argv[4] ) );
        gpio_init( descr[p].gpio,  m );
        adc_init( descr[p].adc, s );
        config[p].mode = PINMODE_ADC;
        config[p].adcsamp = s;
    }

    else if( !strcmp(argv[2], "pwm") ){
        if( descr[p].timer == -1 ){
            fprintf(STDERR, "pwm not supported on pin %s\n", argv[1]);
            return 0;
        }
        short m = GPIO_AF( descr[p].timeraf ) | GPIO_PUSH_PULL | GPIO_SPEED_25MHZ;
        short v = 255;
        int   f = 10000;
        short c;

        for(c=3; c<argc-1; c++){
            if( !strcmp(argv[c], "freq") ) f = atoi(argv[c+1]);
            if( !strcmp(argv[c], "max") )  v = atoi(argv[c+1]);
        }

        gpio_init( descr[p].gpio,  m );
        pwm_init(  descr[p].timer, f, v );
        config[p].mode = PINMODE_PWM;
        config[p].pwmfreq = f;
        config[p].pwmmax  = v;
    }

    else{
        fprintf(STDERR, "invalid mode\n");
    }

    return 0;

}

DEFCONFUNC(conf_pins, f)
{
    short i;
    for(i=0; i<NUM_PINS; i++){

        switch(config[i].mode){
        case PINMODE_IN:
            fprintf(f, "pinmode %s input", descr[i].name);
            if( config[i].gpcf & GPIO_PULL_UP ) fprintf(f, " pullup");
            if( config[i].gpcf & GPIO_PULL_DN ) fprintf(f, " pulldown");
            fprintf(f, "\n");
            break;

        case PINMODE_OUT:
            fprintf(f, "pinmode %s output", descr[i].name);
            if( config[i].gpcf & GPIO_PULL_UP ) fprintf(f, " pullup");
            else if( config[i].gpcf & GPIO_OPEN_DRAIN ) fprintf(f, " opendrain");
            else fprintf(f, " pushpull");
            fprintf(f, "\n");
            break;

        case PINMODE_ADC:
            fprintf(f, "pinmode %s adc samples %d\n", descr[i].name, config[i].adcsamp);
            break;

        case PINMODE_PWM:
            fprintf(f, "pinmode %s pwm freq %d max %d\n", descr[i].name, config[i].pwmfreq, config[i].pwmmax);
            break;
        }
    }
}

/****************************************************************/
int
getpini(int p){
    switch(config[p].mode){
    case PINMODE_IN:
    case PINMODE_OUT:
        return gpio_get( descr[p].gpio );
    case PINMODE_ADC:
        return adc_get( descr[p].adc );
    case PINMODE_PWM:
        return 0;
    }
}

void
setpini(int p, int value){
    switch(config[p].mode){
    case PINMODE_IN:
        return;
    case PINMODE_OUT:
        if( value )
            gpio_set( descr[p].gpio );
        else
            gpio_clear( descr[p].gpio );
        break;
    case PINMODE_ADC:
        return;
    case PINMODE_PWM:
        pwm_set( descr[p].timer, value );
        break;
    }
}

const char *
getpinname(int p){
    return descr[p].name;
}

int
getpin(const char *pin){
    short p = find_pin(pin);
    if( p == -1 ){
        fprintf(STDERR, "invalid pin '%s'\n", pin);
        return 0;
    }

    return getpini(p);
}

void
setpin(const char *pin, int value){
    short p = find_pin(pin);
    if( p == -1 ){
        fprintf(STDERR, "invalid pin '%s'\n", pin);
        return 0;
    }

    setpini(p, value);
}

void
getpins_prepare(uint32_t acqval){
    // RSN - configure adc - bulk + dma
}

void
getpins_all(uint32_t mask, short *values){
    int8_t i;

#ifdef USE_ADCDMA
    utime_t t0 = get_hrtime();
    if( mask != adc_acqmask ){
        // calc only if there's a change
        adc_dma_sqr_init( mask );
        adc_acqmask = mask;
    }
    utime_t t1 = get_hrtime();

    // do all conversions
    adc_dma_get_all();
    utime_t t2 = get_hrtime();

// copy out data
    int8_t d=0;
    for(i=0; i<NUM_PINS; i++){
        if( mask & (1<<i) ){
            switch( config[i].mode ){
            case PINMODE_ADC:
                values[i] = dmabuf[d++];
                break;
            default:
                values[i] = getpini( i );
                break;
            }
        }
    }
    utime_t t3 = get_hrtime();
    printf("get all %d, %d, %d\n", (int)(t1-t0), (int)(t2-t1), (int)(t3-t2));
#else
    for(i=0; i<NUM_PINS; i++){
        if( mask & (1<<i) ) values[i] = getpini( i );
    }
#endif
}


DEFUN(getpin, "get pin value")
{
    if( argc < 2 ){
        fprintf(STDERR, "error: getpin pin\n");
        return 0;
    }

    printf("%d\n", getpin(argv[1]));
    return 0;
}

DEFUN(setpin, "set pin value")
{
    if( argc < 3 ){
        fprintf(STDERR, "error: setpin pin value\n");
        return 0;
    }

    setpin(argv[1], atoi(argv[2]));
    return 0;
}

//################################################################
#ifdef USE_ADCDMA

//#define CRUMBS  "adc"
#include "crumbs.h"
//#define DROP_CRUMB(a,b,c)
//#define RESET_CRUMBS()
//#define DUMP_CRUMBS()

#define DMAC		DMA2_Stream0

#define SR_EOC		2
#define SR_OVR		(1<<5)
#define CR2_SWSTART	0x40000000
#define CR2_ADON	1

#define DMASCR_MINC	(1<<10)
#define DMASCR_DIR_M2P  (1<<6)
#define DMASCR_PFCTRL	(1<<5)
#define DMASCR_TEIE	(1<<2)
#define DMASCR_TCIE	(1<<4)
#define DMASCR_HIPRIO	(3<<16)
#define DMASCR_MEM32	(2<<13)
#define DMASCR_DEV32	(2<<11)
#define DMASCR_MEM16	(1<<13)
#define DMASCR_DEV16	(1<<11)
#define DMASCR_EN	1

static lock_t adclock;
static short adcstate;
#define ADC_DONE	 0
#define ADC_BUSY	 1
#define ADC_ERROR	-1


static inline void
_enable_dma(int cnt){

    DMAC->CR    &= ~1;		// clear EN first
    DMA2->LIFCR |= 0x3D;	// clear irq

    // wait for it to disable
    while( DMAC->CR & 1 )
        ;

    DMAC->CR   = 0;

    DMAC->FCR   = DMAC->FCR & ~3;
    DMAC->FCR  |= 1<<2;
    DMAC->PAR   = (u_long) & ADC1->DR;
    DMAC->M0AR  = (u_long) & dmabuf;
    DMAC->NDTR  = cnt;

    DMAC->CR   |= DMASCR_TEIE | DMASCR_TCIE | DMASCR_MINC | DMASCR_HIPRIO | DMASCR_MEM16 | DMASCR_DEV16
        | (0<<25)	// channel 0
        ;

    ADC1->CR2 |= 1<<8; // DMA
    DMAC->CR  |= 1;     // enable

    DROP_CRUMB("ena dma", DMA2->LISR, ADC1->SR);

}

static inline void
_disable_dma(void){

    DROP_CRUMB("dis dma", DMA2->LISR,DMAC->NDTR);

    ADC1->CR2 &= ~(1<<8);
    DMAC->CR &= ~(DMASCR_EN | DMASCR_TEIE | DMASCR_TCIE);
    DMA2->LIFCR |= 0x3D;	// clear irq
}

static void
adc_dma_init(void){

    DROP_CRUMB("dma init", 0, 0);
    ADC1->CR1    |= 1<<8;	// scan mode
    RCC->AHB1ENR |= 1<<22;	// DMA2
    nvic_enable( IRQ_DMA2_STREAM0, 0x40 );
}

static void
adc_dma_sqr_init(int acqmask){
    short i=0,sq=0;

    // precalculate dma config
    adc_count = 0;
    dmasqr[0] = dmasqr[1] = dmasqr[2] = 0;

    for(i=0; i<NUM_PINS; i++){
        if( config[i].mode != PINMODE_ADC ) continue;
        if( !( acqmask & (1 << i)) ) continue;
        adc_count ++;
        short adc = descr[i].adc;
        short sh  = 5 * (sq % 6);

        switch( sq / 6 ){
        case 0:
            dmasqr[2] |= adc << sh;
            break;
        case 1:
            dmasqr[1] |= adc << sh;
            break;
        case 2:
            dmasqr[0] |= adc << sh;
            break;
        }
        sq ++;
    }

    dmasqr[0] |= (adc_count - 1) << 20;
    adc_dma_init();
}

static void
adc_dma_get_all(void){

    //sync_lock( &adclock, "adc.L" );

    ADC1->SR = ADC1->SR & ~0x3f;

    RESET_CRUMBS();
    DROP_CRUMB("adc start", ADC1->SR, adc_count);
    bzero(dmabuf, sizeof(dmabuf));

    ADC1->SQR3 = dmasqr[2];
    ADC1->SQR2 = dmasqr[1];
    ADC1->SQR1 = dmasqr[0];

    _enable_dma(adc_count);
    adcstate = ADC_BUSY;
    ADC1->CR2 |= CR2_SWSTART;	// Go!
    DROP_CRUMB("adc started", ADC1->SR, DMA2->LISR);

    utime_t to = get_hrtime() + 10000;
    while(1){
        asleep( &adc_dma_get_all, "adc");
        if( adcstate != ADC_BUSY ) break;
        await( -1, 1000 );
        if( adcstate != ADC_BUSY ) break;
        if( get_hrtime() >= to ){
            DROP_CRUMB("toed", err, ADC1->SR);
            adcstate = ADC_ERROR;
        }
    }
    aunsleep();

    _disable_dma();

    if( adcstate == ADC_ERROR ){
        printf("adc error\n");
    }
    DROP_CRUMB("adc done", 0,0);
    //DUMP_CRUMBS();

    //sync_unlock( &adclock );
}

void
DMA2_Stream0_IRQHandler(void){
    int isr = DMA2->LISR & 0x3F;
    DMA2->LIFCR |= 0x3D;	// clear irq

    DROP_CRUMB("dmairq", isr, 0);
    if( isr & 8 ){
        // error - done
        _disable_dma();
        adcstate = ADC_ERROR;
        DROP_CRUMB("dma-err", 0,0);
        wakeup( &adc_dma_get_all );
        return;
    }

    if( isr & 0x20 ){
        // dma complete
        if( ((u_long)dmabuf == DMAC->M0AR) && ! DMAC->NDTR ){
            _disable_dma();
            adcstate = ADC_DONE;
            DROP_CRUMB("dma-done", 0,0);
            wakeup( &adc_dma_get_all );
        }
    }
}


#endif
