/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-04 10:14 (EDT)
  Function: power management

*/


#include <conf.h>
#include <proc.h>
#include <i2c.h>
#include <gpio.h>
#include <stm32.h>
#include <userint.h>

#include "board.h"
#include "util.h"
#include "adp5062.h"
#include "max17048.h"

static void batmon(void);

static char probeadp[2], probemax[2];
static bool have_adp=0, have_max=0;
static bool kicked=0;
static proc_t mon_pid = 0;

static short current_charge  = 100;
static short current_input   = 500;
static short current_trickle = 10;
static short voltage_term    = 4180;	// mV

static struct {
    uint8_t status1;
    uint8_t status2;
    uint8_t fault;
} adpinfo;

static struct {
    uint16_t vcell;
    uint16_t soc;
    uint16_t crate;
} maxinfo;


static i2c_msg pwrinit[] = {
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_VIN,       0x06 ),	// 500mA input limit
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_TERM,      0x8C ),	// 4.2V termination, 3.2V limit
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_CHCURRENT, 0x05 ),	// 100mA fast charge current, 10mA trickle
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_TERM,      0x8F ),	// term @4.2V, 3.8V
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_VTHRESH,   0xEB ),	// recharge enable, defaults
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_TIMER,     0x38 ),	// default timers
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_FUNC1,     0x05 ),	// enable charging
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_FUNC2,     0x07 ),	// 5.0V
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_SHORT,     0x64 ),	// 10 seconds @ 2.4V
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_IEND,      0x10 ),	// term current = ichg/20

    I2C_MSG_C3( MAX17048_ADDRESS,  0, MAX17048_REGISTER_MODE,     0x00, 0x00 ),	//default mode
    I2C_MSG_C3( MAX17048_ADDRESS,  0, MAX17048_REGISTER_CONFIG,   0x97, 0x1C ),	//default config

};

static i2c_msg pwroff[] = {
    I2C_MSG_C2( ADP5062_ADDRESS,   0, ADP5062_REGISTER_FUNC1,       0x0 ),
    // QQQ - many options. turn charging off? ldo?
    // NB - max17048 will hibernate by itself
};

static i2c_msg adpprobe[] = {
    I2C_MSG_C1( ADP5062_ADDRESS,   0,             ADP5062_REGISTER_MODEL ),
    I2C_MSG_DL( ADP5062_ADDRESS,   I2C_MSGF_READ, 2, probeadp ),
};

static i2c_msg maxprobe[] = {
    I2C_MSG_C1( MAX17048_ADDRESS,   0,             MAX17048_REGISTER_VERS ),
    I2C_MSG_DL( MAX17048_ADDRESS,   I2C_MSGF_READ, 2, probemax ),
};

static i2c_msg pwrreadall[] = {
    I2C_MSG_C1( ADP5062_ADDRESS,   0,             ADP5062_REGISTER_STATUS1 ),
    I2C_MSG_DL( ADP5062_ADDRESS,   I2C_MSGF_READ, 3, (void*)&adpinfo ),

    I2C_MSG_C1( MAX17048_ADDRESS,   0,             MAX17048_REGISTER_VCELL ),
    I2C_MSG_DL( MAX17048_ADDRESS,   I2C_MSGF_READ, 2, (void*)&maxinfo.vcell ),

    I2C_MSG_C1( MAX17048_ADDRESS,   0,             MAX17048_REGISTER_SOC ),
    I2C_MSG_DL( MAX17048_ADDRESS,   I2C_MSGF_READ, 2, (void*)&maxinfo.soc ),

    I2C_MSG_C1( MAX17048_ADDRESS,   0,             MAX17048_REGISTER_CRATE ),
    I2C_MSG_DL( MAX17048_ADDRESS,   I2C_MSGF_READ, 2, (void*)&maxinfo.crate ),
};


void
pmic_init(void){

    char i;
    for(i=0; i<10; i++){
        // init
        i2c_xfer(I2CUNIT, ELEMENTSIN(pwrinit), pwrinit, 1000000);
        // try to read
        i2c_xfer(I2CUNIT, ELEMENTSIN(adpprobe), adpprobe, 100000);
        i2c_xfer(I2CUNIT, ELEMENTSIN(maxprobe), maxprobe, 100000);

        if( probeadp[0] && !have_adp ){
            have_adp = 1;
            bootmsg("pmic adp5062 at i2c%d addr %x model %x rev %x\n", I2CUNIT, ADP5062_ADDRESS, probeadp[0], probeadp[1]);
        }
        if( (probemax[0] || probemax[1]) && !have_max ){
            have_max = 1;
            bootmsg("gauge max17048 at i2c%d addr %x rev %x\n", I2CUNIT, MAX17048_ADDRESS, probemax[1]);
        }

        // NB: max17048 will not probe without a battery attached

        if( have_adp /*&& have_max */ ) break;
    }

    if( !mon_pid )
        mon_pid = start_proc(512, batmon, "batmon");
}

static void
max_reprobe(void){

    i2c_xfer(I2CUNIT, ELEMENTSIN(maxprobe), maxprobe, 100000);
    if( (probemax[0] || probemax[1]) && !have_max ){
        have_max = 1;
        bootmsg("gauge max17048 at i2c%d addr %x rev %x\n", I2CUNIT, MAX17048_ADDRESS, probemax[1]);
    }
}


void
pmic_disable(void){
    i2c_xfer(I2CUNIT, ELEMENTSIN(pwroff), pwroff, 1000000);
}

int
pmic_test(void){

    if( !have_adp ) pmic_init();
    if( !have_adp ) return 0;
    // ...
    return 1;
}


void
read_pmic(void){
    i2c_xfer(I2CUNIT, ELEMENTSIN(pwrreadall), pwrreadall, 100000);
}

#if 0
DEFUN(pmicprobe, "probe pmic")
{
    int i;
    int allmax=0, accmax=0, gyrmax=0;
    int have_accel = 0;

    probeadp[0] = probemax[0] = 0;
    i2c_xfer(I2CUNIT, ELEMENTSIN(adpprobe), adpprobe, 100000);
    i2c_xfer(I2CUNIT, ELEMENTSIN(maxprobe), maxprobe, 100000);

    printf("adp %x,%x\n", probeadp[0], probeadp[1]);
    printf("max %x,%x\n", probemax[0], probemax[1]);

    return 0;
}
#endif

#define SWAP(a)		((((a)&0xFF)<<8) | ((a)>>8))

DEFUN(pmictest, "test pmic")
{

    pmic_init();
    read_pmic();
    printf("adp s1 %x s2 %x f %x\n",
           adpinfo.status1, adpinfo.status2, adpinfo.fault);

    printf("max vc %x soc %x cr %x\n",
           SWAP(maxinfo.vcell), SWAP(maxinfo.soc), SWAP(maxinfo.crate));

    return 0;
}

/****************************************************************/
static const char *batt_status[] = {
    "off",
    "no batt",
    "dead",
    "weak",
    "okay",
    "?5", "?6", "?7",
};

static const char *charg_status[] = {
    "off",
    "trickle",
    "chrging",	// fast CC
    "topping",	// fast CV
    "finishd",
    "LDO on",
    "expired",
    "waiting",	// looking for battery
};

static void set_input_current(int);

DEFUN(charge_status, "show charging status")
{

    while(1){
        read_pmic();
        read_imu_quick();

        const char *err = 0;

        if( adpinfo.status1 & (1<<5) ){
            err = "low current";
        }
        if( adpinfo.status1 & (1<<4) ){
            err = "too hot";
        }
        if( adpinfo.fault & (1<<3) ){
            err = "batt short";
        }
        if( adpinfo.status1 & (1<<7) ){
            err = "Vin Overld";
        }

        short soc = maxinfo.soc & 0xFF;

        if( argv[0][0] == '-' )
            printf("\e[J\e[17m");	// 10x20 font

        printf("CHG: %s\n", charg_status[ adpinfo.status1 & 7 ]);

        if( (adpinfo.status2 & 7) == 4 || (adpinfo.status1 & 7) == 0 )
            printf("BAT: %d%%\n", soc);
        else
            printf("BAT: %s\n", batt_status[  adpinfo.status2 & 7 ]);

        if( err )
            // RSN - in red
            printf("! %s\n", err);

        if( argv[0][0] != '-' ) break;
        if( check_button_or_upsidedown() ) break;
        usleep(250000);

        if( kicked ){
            kicked = 0;
            sleep(2);
            continue;
        }
    }

    return 0;
}


int
power_level(void){

    read_pmic();

    // charger attached? => 100%
    if( (adpinfo.status1 & 7) != ADP5062_S1_CHG_OFF ) return 100;
    // else => battery level
    return maxinfo.soc & 0xFF;

}


/****************************************************************/

static const short input_values[] = {
    100, 150, 200, 250, 300, 400, 500, 600, 700, 800, 900, 1000, 1200, 1500, 1800, 2100
};
static const short charge_values[] = {
    50, 100, 150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750,
    800, 850, 900, 950, 1000, 1050, 1100, 1200, 1300
};
static const short trickle_values[] = {
    5, 10, 20, 80
};

static int
find_index(int val, const short *table, int size){

    while(--size){
        if( table[size] <= val ) return size;
    }

    return 0;
}

static i2c_msg tmpmsg[2];

int
get_reg(int dev, int reg, int b){
    u_char buf[2];

    tmpmsg[0].slave    = dev;
    tmpmsg[0].flags    = 0;
    tmpmsg[0].clen     = 1;
    tmpmsg[0].dlen     = 0;
    tmpmsg[0].cdata[0] = reg;
    tmpmsg[0].data     = 0;
    tmpmsg[1].slave    = dev;
    tmpmsg[1].flags    = I2C_MSGF_READ;
    tmpmsg[1].clen     = 0;
    tmpmsg[1].dlen     = b;
    tmpmsg[1].data     = (void*)&buf;

    i2c_xfer(I2CUNIT, 2, tmpmsg, 100000);

    if( b == 1 ) return buf[0];
    return (buf[0]<<8) | buf[1];
}

void
set_reg(int dev, int reg, int b, int v){
    tmpmsg[0].slave    = dev;
    tmpmsg[0].flags    = 0;
    tmpmsg[0].clen     = 1 + b;
    tmpmsg[0].dlen     = 0;
    tmpmsg[0].cdata[0] = reg;
    tmpmsg[0].data     = 0;

    if( b == 1 )
        tmpmsg[0].cdata[1] = v;
    else{
        tmpmsg[0].cdata[1] = (v >> 8) & 0xFF;
        tmpmsg[0].cdata[2] = v & 0xFF;
    }

    i2c_xfer(I2CUNIT, 1, tmpmsg, 100000);

}


void
set_input_current(int i){
    short s = find_index(i, input_values, ELEMENTSIN(input_values));
    current_input = input_values[s];

    short v = get_reg(ADP5062_ADDRESS, ADP5062_REGISTER_VIN, 1);
    v = s;

    set_reg(ADP5062_ADDRESS, ADP5062_REGISTER_VIN, 1, v);
}

void
set_charge_current(int i){
    short s = find_index(i, charge_values, ELEMENTSIN(charge_values));
    current_charge = charge_values[s];

    short v = get_reg(ADP5062_ADDRESS, ADP5062_REGISTER_CHCURRENT, 1);
    v &= 3;
    v |= s<<2;

    set_reg(ADP5062_ADDRESS, ADP5062_REGISTER_CHCURRENT, 1, v);
}

void
set_trickle_current(int i){
    short s = find_index(i, trickle_values, ELEMENTSIN(trickle_values));
    current_trickle = trickle_values[s];

    short v = get_reg(ADP5062_ADDRESS, ADP5062_REGISTER_CHCURRENT, 1);
    v &= ~3;
    v |= s;

    set_reg(ADP5062_ADDRESS, ADP5062_REGISTER_CHCURRENT, 1, v);

}


/****************************************************************/

DEFUN(input_current, "set maximum input current")
{

    if( argc == 2 ){
        set_input_current( atoi(argv[1]) );
    }else if( argv[0][0] == '-' ){
        short s = find_index(current_input, input_values, ELEMENTSIN(input_values));
        short v = menu_get_pshort("I_input", "mA max USB current", ELEMENTSIN(input_values), input_values, s );
        if( v != -1 ) set_input_current( input_values[v] );
    }else{
        printf("input current: %d\n", current_input);
    }

    return 0;
}

DEFUN(charge_current, "set charge current")
{
    if( argc == 2 ){
        set_charge_current( atoi(argv[1]) );
    }else if( argv[0][0] == '-' ){
        int s = find_index(current_charge, charge_values, ELEMENTSIN(charge_values));
        int v = menu_get_pshort("I_charge", "mA max charge current", ELEMENTSIN(charge_values), charge_values, s );
        if( v != -1 ) set_charge_current( charge_values[v] );
    }else{
        printf("charge current: %d\n", current_charge);
    }

    return 0;
}

DEFUN(trickle_current, "set charge trickle current")
{
    if( argc == 2 ){
        set_trickle_current( atoi(argv[1]) );
    }else if( argv[0][0] == '-' ){
        int s = find_index(current_trickle, trickle_values, ELEMENTSIN(trickle_values));
        int v = menu_get_pshort("I_trickle", "mA trickle current", ELEMENTSIN(trickle_values), trickle_values, s );
        if( v != -1 ) set_trickle_current( trickle_values[v] );
    }else{
        printf("trickle current: %d\n", current_trickle);
    }

    return 0;
}

DEFCONFUNC(charger, f)
{
    fprintf(f, "input_current %d\n",   current_input);
    fprintf(f, "charge_current %d\n",  current_charge);
    fprintf(f, "trickle_current %d\n", current_trickle);
}

static void
kickit(void){
    kicked = 1;
    short v = get_reg(ADP5062_ADDRESS, ADP5062_REGISTER_FUNC1, 1);
    v &= ~1;
    set_reg(ADP5062_ADDRESS, ADP5062_REGISTER_FUNC1, 1, v);
    usleep(10000);

    v |= 1;
    set_reg(ADP5062_ADDRESS, ADP5062_REGISTER_FUNC1, 1, v);

    return 0;
}

DEFUN(charge_reset, "reset charger")
{
    kickit();
    return 0;
}

static void
batmon(void){

    currproc->prio = 14;
    while(1){
        sleep(15);
        read_pmic();
        short as = get_reg(ADP5062_ADDRESS, ADP5062_REGISTER_SHORT, 1);

        // fast-CV => kick

        // max17048 won't probe without a battery
        // if we have a battery, retry
        if( !have_max && (adpinfo.status2 & 7) == ADP5062_S2_BAT_OKAY ){
            max_reprobe();
        }

        // if we lose Vusb, the config gets reset. put it back.
        // there is no latching bit for loss of Vin
        // there is an interrupt, but no pin...
        // => see if the registers look different
        if( as == ADP5062_SHORT_DEFAULT ){
            i2c_xfer(I2CUNIT, ELEMENTSIN(pwrinit), pwrinit, 1000000);
            set_input_current(   current_input );
            set_charge_current(  current_charge );
            set_trickle_current( current_trickle );
        }

        // the adp5062 does not detect battery disconnect during fast-CV
        // if we are in fast-CV, kick it to be sure
        //if( (adpinfo.status1 & 7) == ADP5062_S1_CHG_FASTCV )
        //    kickit();

    }
}


#ifdef KTESTING
DEFUN(pmicdump, "")
{
    int i;
    printf("adp:");
    for(i=0; i<=0x11; i++){
        int v = get_reg(ADP5062_ADDRESS, i, 1);
        printf(" %02.2X", v);
    }
    printf("\n");
    printf("max:");
    for(i=0; i<=0x1A; i+=2){
        int v = get_reg(MAX17048_ADDRESS, i, 2);
        printf(" %02.2X %02.2X", v>>8, v&0xFF);
    }

    printf("\n");
    return 0;
}
#endif
