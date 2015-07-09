/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-02 21:37 (EDT)
  Function: 

*/

#define I2CUNIT			0

#define HWCF_GPIO_BUTTON        GPIO_C13
#define HWCF_GPIO_DPY_CD        GPIO_B10
#define HWCF_GPIO_DPY_CS        GPIO_B12
#define HWCF_GPIO_SD_CS         GPIO_A8
#define HWCF_GPIO_SD_DET        GPIO_A15
#define HWCF_GPIO_NPWREN	GPIO_B5
#define HWCF_GPIO_IMUINT	GPIO_B4		// on v1 only
#define HWCF_GPIO_NRST2		GPIO_B3		// on v2

#define HWCF_GPIO_LED_WHITE     GPIO_B8
#define HWCF_GPIO_AUDIO         GPIO_B9

#define HWCF_TIMER_LED_WHITE    TIMER_4_3
#define HWCF_TIMER_AUDIO        TIMER_4_4


#if defined(AMIKETO_v1) || defined(AMIKETO_v2)
# define HAVE_LIS3DH
#else
# define HAVE_LSM303D
#endif

#if defined(AMIKETO_v2)
// no charger or fuel gauge
#else
# define HAVE_PMIC
#endif

