

#define PINMODE_NONE	0
#define PINMODE_ADC	1
#define PINMODE_IN	2
#define PINMODE_OUT	3
#define PINMODE_PWM	4
#define PINMODE_DAC	5
#define PINMODE_TOUCH	6

extern void        getpins_all(uint32_t, short *);
extern int         getpintypei(int);
extern int         find_pin(const char *);
extern const char *getpinname(int);


