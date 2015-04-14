



#define ADP5062_ADDRESS		(0x28 >> 1)

#define ADP5062_REGISTER_MODEL	   0x00
#define ADP5062_REGISTER_REVISION  0x01
#define ADP5062_REGISTER_VIN       0x02		// input current-limit, default 100mA
#define ADP5062_REGISTER_TERM      0x03		// termination voltage, default 4.2V
#define ADP5062_REGISTER_CHCURRENT 0x04		// fast charge current, default 750mA; trickle current, default 20mA
#define ADP5062_REGISTER_VTHRESH   0x05
#define ADP5062_REGISTER_TIMER     0x06
#define ADP5062_REGISTER_FUNC1     0x07
#define ADP5062_REGISTER_FUNC2     0x08
#define ADP5062_REGISTER_INTEN     0x09
#define ADP5062_REGISTER_INTACT    0x0A
#define ADP5062_REGISTER_STATUS1   0x0B
#define ADP5062_REGISTER_STATUS2   0x0C
#define ADP5062_REGISTER_FAULT     0x0D
#define ADP5062_REGISTER_SHORT     0x10
#define ADP5062_REGISTER_IEND      0x11

#define ADP5062_S1_CHG_OFF      0
#define ADP5062_S1_CHG_TRICKLE  1
#define ADP5062_S1_CHG_FASTCC   2
#define ADP5062_S1_CHG_FASTCV   3
#define ADP5062_S1_CHG_COMPLETE 4
#define ADP5062_S1_CHG_LDO      5
#define ADP5062_S1_CHG_EXPIRED  6
#define ADP5062_S1_CHG_BATDET   7

#define ADP5062_S2_BAT_OFF   0
#define ADP5062_S2_BAT_NOBAT 1
#define ADP5062_S2_BAT_DEAD  2
#define ADP5062_S2_BAT_WEAK  3
#define ADP5062_S2_BAT_OKAY  4

// we change this, so we can detect reset
#define ADP5062_SHORT_DEFAULT	0x84

