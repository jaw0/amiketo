/*
  Copyright (c) 2013
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2013-Aug-31 13:29 (EDT)
  Function: lsm303d
*/

#include <conf.h>
#include <proc.h>
#include <i2c.h>
#include <gpio.h>
#include <stm32.h>
#include <userint.h>
#include "board.h"
#include "util.h"

#include "lsm303d.h"
#include "lis3dh.h"

#define ui_pause()
#define ui_resume()


static char magbuf[6];	  // x, z, y: hi,lo
static char accbuf[6];	  // x, y, z: lo,hi
static char lsmtmpbuf[2]; // hi,lo
static char probeaccel[1];
static bool have_accel = 0;

static short accel_off_x, accel_off_y, accel_off_z;

#if defined(HAVE_LSM303D)
static i2c_msg imuinit[] = {
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL0, 0 ),
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL1, 0xA7 ),	// enable acc, 1600Hz
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL2, 0 ),	// 773Hz filter, +- 2g
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL3, 0 ),
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL4, 0 ),
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL5, 0xF0 ),	// temp enable, mag hires 50Hz
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL6, 0x20 ),	// +- 4gauss
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL7, 0 ),	// continuous mode
};

static i2c_msg imuoff[] = {
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL1, 0 ),	// disable acc
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL5, 0 ),	// disable temp
    I2C_MSG_C2( LSM303D_ADDRESS,   0, LSM303D_REGISTER_CTRL7, 7 ),	// disable mag
};

static i2c_msg imuprobe[] = {
    I2C_MSG_C1( LSM303D_ADDRESS,   0,             LSM303D_REGISTER_WHOAMI ),
    I2C_MSG_DL( LSM303D_ADDRESS,   I2C_MSGF_READ, 1, probeaccel ),
};

static i2c_msg imureadall[] = {
    I2C_MSG_C1( LSM303D_ADDRESS,   0,             LSM303D_REGISTER_OUT_X_L_M | 0x80 ),
    I2C_MSG_DL( LSM303D_ADDRESS,   I2C_MSGF_READ, 6, magbuf ),

    I2C_MSG_C1( LSM303D_ADDRESS,   0,             LSM303D_REGISTER_OUT_X_L_A | 0x80 ),
    I2C_MSG_DL( LSM303D_ADDRESS,   I2C_MSGF_READ, 6, accbuf ),

    I2C_MSG_C1( LSM303D_ADDRESS,   0,             LSM303D_REGISTER_TEMP_OUT_L | 0x80 ),
    I2C_MSG_DL( LSM303D_ADDRESS,   I2C_MSGF_READ, 2, lsmtmpbuf ),
};

static i2c_msg imureadmost[] = {
    I2C_MSG_C1( LSM303D_ADDRESS,   0,             LSM303D_REGISTER_OUT_X_L_A | 0x80 ),
    I2C_MSG_DL( LSM303D_ADDRESS,   I2C_MSGF_READ, 6, accbuf ),
};

static i2c_msg imureadquick[] = {
    I2C_MSG_C1( LSM303D_ADDRESS,   0,             LSM303D_REGISTER_OUT_X_L_A | 0x80 ),
    I2C_MSG_DL( LSM303D_ADDRESS,   I2C_MSGF_READ, 6, accbuf ),
};

#elif defined(HAVE_LIS3DH)
static i2c_msg imuinit[] = {
    I2C_MSG_C2( LIS3DH_ADDRESS,   0, LIS3DH_REGISTER_TEMP_CFG, 0x40 ),	// enable temp sensor
    I2C_MSG_C2( LIS3DH_ADDRESS,   0, LIS3DH_REGISTER_CTRL1,    0x97 ),	// 1.25kHz DR, enable acc
    I2C_MSG_C2( LIS3DH_ADDRESS,   0, LIS3DH_REGISTER_CTRL2,    0 ),	// no high-pass
    I2C_MSG_C2( LIS3DH_ADDRESS,   0, LIS3DH_REGISTER_CTRL3,    0 ),	// no ints
    I2C_MSG_C2( LIS3DH_ADDRESS,   0, LIS3DH_REGISTER_CTRL4,    0x08 ),	// continuous, little-endian, +-2g, hi-res
    I2C_MSG_C2( LIS3DH_ADDRESS,   0, LIS3DH_REGISTER_CTRL5,    0x0 ),	// no fifo
    I2C_MSG_C2( LIS3DH_ADDRESS,   0, LIS3DH_REGISTER_CTRL6,    0x0 ),	// undocumented
};

static i2c_msg imuoff[] = {
    I2C_MSG_C2( LIS3DH_ADDRESS,   0, LIS3DH_REGISTER_CTRL1,    0 ),	// power down mode
    I2C_MSG_C2( LIS3DH_ADDRESS,   0, LIS3DH_REGISTER_TEMP_CFG, 0 ),	// disable temp
};

static i2c_msg imuprobe[] = {
    I2C_MSG_C1( LIS3DH_ADDRESS,   0,             LIS3DH_REGISTER_WHOAMI ),
    I2C_MSG_DL( LIS3DH_ADDRESS,   I2C_MSGF_READ, 1, probeaccel ),
};

static i2c_msg imureadall[] = {
    I2C_MSG_C1( LIS3DH_ADDRESS,   0,             LIS3DH_REGISTER_OUT_X_L | 0x80 ),
    I2C_MSG_DL( LIS3DH_ADDRESS,   I2C_MSGF_READ, 6, accbuf ),

    // QQQ - read adc channels?
};

static i2c_msg imureadmost[] = {
    I2C_MSG_C1( LIS3DH_ADDRESS,   0,             LIS3DH_REGISTER_OUT_X_L | 0x80 ),
    I2C_MSG_DL( LIS3DH_ADDRESS,   I2C_MSGF_READ, 6, accbuf ),
};

static i2c_msg imureadquick[] = {
    I2C_MSG_C1( LIS3DH_ADDRESS,   0,             LIS3DH_REGISTER_OUT_X_L | 0x80 ),
    I2C_MSG_DL( LIS3DH_ADDRESS,   I2C_MSGF_READ, 6, accbuf ),
};


#else
#  error "no imu"
#endif

/****************************************************************/

void
imu_init(void){
    char i;
    for(i=0; i<10; i++){
        // init
        i2c_xfer(I2CUNIT, ELEMENTSIN(imuinit), imuinit, 1000000);
        // try to read
        i2c_xfer(I2CUNIT, ELEMENTSIN(imuprobe), imuprobe, 100000);

#if defined(HAVE_LSM303D)
        if( probeaccel[0] == LSM303D_WHOAMI ){
            have_accel = 1;
            bootmsg("imu lsm303d at i2c%d addr %x\n", I2CUNIT, LSM303D_ADDRESS);
            break;
        }
#elif defined(HAVE_LIS3DH)
        if( probeaccel[0] == LIS3DH_WHOAMI ){
            have_accel = 1;
            bootmsg("imu lis3dh at i2c%d addr %x\n", I2CUNIT, LIS3DH_ADDRESS);
            break;
        }
#endif
    }
}

void
imu_disable(void){
    i2c_xfer(I2CUNIT, ELEMENTSIN(imuoff), imuoff, 1000000);
}

int
imu_test(void){

    if( !have_accel ) imu_init();
    if( !have_accel ) return 0;

    // sanity check
    read_imu_all();
    read_imu_all();
    int ax = accel_x();
    int ay = accel_y();
    int az = accel_z();
    int a  = ABS(ax) + ABS(ay) + ABS(az);

    if( a > 500 ) return 1;
    return 0;

}

#if 1
static int i2c_speed[] = { /*900000, 800000, 700000,  600000, 500000,*/ 400000, 300000, 200000, 100000 };

DEFUN(imuprobe, "probe imu")
{
    int i;
    int allmax=0, accmax=0, gyrmax=0;
    int have_accel = 0;

    ui_pause();

    for(i=0; i<ELEMENTSIN(i2c_speed); i++){
        i2c_set_speed(I2CUNIT, i2c_speed[i] );
        i2c_xfer(I2CUNIT, ELEMENTSIN(imuinit), imuinit, 1000000);

        probeaccel[0] = 0;
        i2c_xfer(I2CUNIT, ELEMENTSIN(imuprobe), imuprobe, 100000);

        if( probeaccel[0] ){
            have_accel = 1;
            if( i2c_speed[i] > accmax ) accmax = i2c_speed[i];
        }
        printf("%d\n", i);
    }

    ui_resume();
    printf("accel %d max %d\n", have_accel, accmax);
    return 0;
}
#endif

void
read_imu_all(void){
    i2c_xfer(I2CUNIT, ELEMENTSIN(imureadall), imureadall, 100000);
}

void
read_imu_quick(void){
    i2c_xfer(I2CUNIT, ELEMENTSIN(imureadquick), imureadquick, 100000);
}

void
read_imu_most(void){
    i2c_xfer(I2CUNIT, ELEMENTSIN(imureadmost), imureadmost, 100000);
}

/****************************************************************/
static inline int
_accel_x(void){
    short ax = ((accbuf[1]<<8) | accbuf[0]);
    ax >>= 4;
    return ax;
}

static inline int
_accel_y(void){
    short ay = ((accbuf[3]<<8) | accbuf[2]);
    ay >>= 4;
    return ay;
}

static inline int
_accel_z(void){
    short az = ((accbuf[5]<<8) | accbuf[4]);
    az >>= 4;
    return az;
}

static inline int
_compass_x(void){
    short ax = ((magbuf[1]<<8) | magbuf[0]);
    return ax;
}

static inline int
_compass_y(void){
    short ay = ((magbuf[3]<<8) | magbuf[2]);
    return ay;
}

static inline int
_compass_z(void){
    short az = ((magbuf[5]<<8) | magbuf[4]);
    return az;
}
/****************************************************************/

#if ACCEL_ROTATE == 0
int accel_x(void){   return   _accel_x() - accel_off_x; }
int accel_y(void){   return   _accel_y() - accel_off_y; }
int compass_x(void){ return   _compass_x(); }
int compass_y(void){ return   _compass_y(); }

#elif ACCEL_ROTATE == 180
int accel_x(void){   return - _accel_x() + accel_off_x; }
int accel_y(void){   return - _accel_y() + accel_off_y; }
int compass_x(void){ return - _compass_x(); }
int compass_y(void){ return - _compass_y(); }

#elif ACCEL_ROTATE == 90
int accel_x(void){   return - _accel_y() + accel_off_y; }
int accel_y(void){   return   _accel_x() - accel_off_x; }
int compass_x(void){ return - _compass_y(); }
int compass_y(void){ return   _compass_x(); }

#elif ACCEL_ROTATE == 270
int accel_x(void){   return   _accel_y() - accel_off_y; }
int accel_y(void){   return - _accel_x() + accel_off_x; }
int compass_x(void){ return   _compass_y(); }
int compass_y(void){ return - _compass_x(); }

#endif

int accel_z(void){   return _accel_z() - accel_off_z; }
int compass_z(void){ return _compass_z(); }

/****************************************************************/
int compass_temp(void){
    short at = ((lsmtmpbuf[1]<<8) | lsmtmpbuf[0]);
    return at;
}

/****************************************************************/
// device must be level + still
void
imu_calibrate(void){
    int axtot=0, aytot=0, aztot=0;
    int n;

    accel_off_x = 0;
    accel_off_y = 0;
    accel_off_z = 0;

    for(n=0; n<1000; n++){
        read_imu_all();
        axtot += _accel_x();
        aytot += _accel_y();
        aztot += _accel_z() - 1000;
        usleep(1000);
    }

    accel_off_x = axtot/1000;
    accel_off_y = aytot/1000;
    accel_off_z = aztot/1000;
}

/****************************************************************/

DEFUN(imutest, "test imu")
{
    while(1){
        read_imu_all();
        printf("\e[J");
        //printf("gyro %6d %6d %6d\n", gyro_x(),    gyro_y(),    gyro_z());
        printf("accl %6d %6d %6d\n", accel_x(),   accel_y(),   accel_z());
        printf("magn %6d %6d %6d\n", compass_x(), compass_y(), compass_z());
        printf("temp %6d\n",         compass_temp());
        printf("\n");

        if( argv[0][0] != '-' ) break;
        if( check_button_or_upsidedown() ) break;

        usleep(250000);
    }
    return 0;
}

