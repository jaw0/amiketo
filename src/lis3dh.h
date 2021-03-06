/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-May-01 00:13 (EDT)
  Function: lis3dh accelerometer

*/

#ifndef __lis3dh_h__
#define __lis3dh_h__

#define LIS3DH_ADDRESS          (0x30 >> 1)         // 0011000x
#define LIS3DH_WHOAMI		    0x33

#define LIS3DH_REGISTER_STATUS_AUX    0x07
#define LIS3DH_REGISTER_OUT_ADC1_L    0x08
#define LIS3DH_REGISTER_OUT_ADC1_H    0x09
#define LIS3DH_REGISTER_OUT_ADC2_L    0x0A
#define LIS3DH_REGISTER_OUT_ADC2_H    0x0B
#define LIS3DH_REGISTER_OUT_ADC3_L    0x0C
#define LIS3DH_REGISTER_OUT_ADC3_H    0x0D
#define LIS3DH_REGISTER_INT_COUNTER   0x0E
#define LIS3DH_REGISTER_WHOAMI        0x0F
#define LIS3DH_REGISTER_TEMP_CFG      0x1F
#define LIS3DH_REGISTER_CTRL1         0x20
#define LIS3DH_REGISTER_CTRL2         0x21
#define LIS3DH_REGISTER_CTRL3         0x22
#define LIS3DH_REGISTER_CTRL4         0x23
#define LIS3DH_REGISTER_CTRL5         0x24
#define LIS3DH_REGISTER_CTRL6         0x25
#define LIS3DH_REGISTER_REFERENCE     0x26
#define LIS3DH_REGISTER_STATUS2       0x27
#define LIS3DH_REGISTER_OUT_X_L       0x28
#define LIS3DH_REGISTER_OUT_X_H       0x29
#define LIS3DH_REGISTER_OUT_Y_L       0x2A
#define LIS3DH_REGISTER_OUT_Y_H       0x2B
#define LIS3DH_REGISTER_OUT_Z_L       0x2C
#define LIS3DH_REGISTER_OUT_Z_H       0x2D
#define LIS3DH_REGISTER_FIFO_CTRL     0x2E
#define LIS3DH_REGISTER_FIFO_SRC      0x2F
#define LIS3DH_REGISTER_INT1_CFG      0x30
#define LIS3DH_REGISTER_INT1_SOURCE   0x31
#define LIS3DH_REGISTER_INT1_THS      0x32
#define LIS3DH_REGISTER_INT1_DURATION 0x33
#define LIS3DH_REGISTER_CLICK_CFG     0x38
#define LIS3DH_REGISTER_CLICK_SRC     0x39
#define LIS3DH_REGISTER_CLICK_THS     0x3A
#define LIS3DH_REGISTER_TIME_LIMIT    0x3B
#define LIS3DH_REGISTER_TIME_LATENCY  0x3C
#define LIS3DH_REGISTER_TIME_WINDOW   0x3D

#endif /*  __lis3dh_h__ */
