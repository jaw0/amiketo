/*
  Copyright (c) 2013
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2013-Apr-18 20:28 (EDT)
  Function: menus, accelerometer for user input
*/


#include <conf.h>
#include <proc.h>
#include <gpio.h>
#include <msgs.h>
#include <pwm.h>
#include <sys/types.h>
#include <ioctl.h>
#include <userint.h>

#include "menu.h"

#define AUTOREPEAT_X	250000
#define AUTOREPEAT_Y	1000000
#define TILT_MIN	500		// ~ 30 degree tilt
#define IDLETIME	10		// seconds until screensaver


/*
u+80	arrowleft
u+81	arrowup
u+82	arrowright
u+83	arrowdown
u+84	arrowupdn
u+85	arrowboth
*/

extern void beep(int, int, int);
extern int ivolume;

static int menu_paused = 0;

#define set_leds_rgb(a,b)

// only when not running on battery
DEFVAR(int, screensaver_enable, 1, UV_TYPE_UL | UV_TYPE_CONFIG, "enable screensaver")

void
ui_sleep(void){
    // low power

    FILE *f = fopen("dev:oled0", "w");
    fioctl(f, IOC_GFXSLEEP, 1);		// oled sleep mode
    menu_paused = 2;
}

void
ui_awake(void){

    FILE *f = fopen("dev:oled0", "w");
    fioctl(f, IOC_GFXSLEEP, 0);
    menu_paused = 0;
}

void
ui_pause(void){
    // prevent usage during disk access
    menu_paused = 1;
}

void
ui_resume(void){
    if( menu_paused > 1 ) ui_awake();
    menu_paused = 0;
}

/****************************************************************/

DEFUN(uisleep, "test ui sleep")
{
    ui_sleep();
}
DEFUN(uiwake, "test ui sleep")
{
    ui_awake();
}

/****************************************************************/


static void
screensaver(void){
    char saver = 0;
    short i=0, y=0, x;

    if( power_level() >= 100 && screensaver_enable ){
        printf("\e[J\e[10m");
        saver = 1;
    }else{
        ui_sleep();
    }
    while(1){
        read_imu_all();
        if( accel_x() > TILT_MIN || accel_x() < -TILT_MIN || accel_y() > TILT_MIN || accel_y() < -TILT_MIN )
            break;
        if( saver ){
#if 0
            printf("\e[%d;1H", y);
            for(x=0; x<26; x++){
                char c = (x+y+i) % 9 ? '\\' : 'Y';

                if( y & 1 ){
                    printf("%c", (y+i-x+8) % 8 ? ' ' : c);
                }else{
                    printf("%c", (y+x+i) % 8   ? ' ' : '/');
                }
            }
            printf("\xB");
            y = (y + 1) % 8;
            if(!y) i++;
#else
            screensaver_step();
#endif
        }
        usleep(20000);
    }
    ui_awake();
}

static clear_line(int dir){
    int i;

    for(i=0; i<26; i++){
        if( dir > 0 )
            printf("\e[<");
        else
            printf("\e[>");
        printf("\xB");
        usleep(10000);
    }
}

static int
get_input(void){
    static int     prevach = 0;
    static utime_t prevat  = 0;
    utime_t now;
    int  flatct = 0;
    char leftct = 0;
    char rightct = 0;
    char upct = 0;
    char dnct = 0;

    while(1){
        if( menu_paused ){
            usleep(100000);
            continue;
        }

        // button = enter
        if( check_button() ){
            return '*';
        }

        read_imu_all();
        int ax = -accel_x();
        int ay = -accel_y();
        now = get_time();

        // flat?
        if( ax < TILT_MIN && ax > -TILT_MIN && ay < TILT_MIN && ay > -TILT_MIN )
            flatct ++;
        else
            flatct = 0;

        if( ax > TILT_MIN )
            upct ++;
        else
            upct = 0;

        if( ax < -TILT_MIN )
            dnct ++;
        else
            dnct = 0;

        if( ay > TILT_MIN )
            leftct ++;
        else
            leftct = 0;

        if( ay < -TILT_MIN )
            rightct ++;
        else
            rightct = 0;

        if( flatct > 10 ){
            prevach = 0;
            prevat  = 0;
        }

        if( flatct > IDLETIME * 100 ){
            screensaver();
            return '?';
        }

        if( rightct > 10 )
            if( !prevach || (prevach == '>' && now - prevat > AUTOREPEAT_X) ){
                beep(400, ivolume, 100000);
                prevat  = now;
                return prevach = '>';
            }

        if( leftct > 10 )
            if( !prevach || (prevach == '<' && now - prevat > AUTOREPEAT_X) ){
                beep(400, ivolume, 100000);
                prevat  = now;
                return prevach = '<';
            }

        if( dnct > 10 )
            if( !prevach || (prevach == '*' && now - prevat > AUTOREPEAT_Y) ){
                beep(600, ivolume, 100000);
                beep(300, ivolume, 150000);
                prevat  = now;
                return prevach = '*';
            }

        if( upct > 10 )
            if( !prevach || (prevach == '^' && now - prevat > AUTOREPEAT_Y) ){
                beep(300, ivolume, 100000);
                beep(320, ivolume, 150000);
                prevat  = now;
                return prevach = '^';
            }

        usleep(10000);
    }
}

static inline void
topline(const char *title, int nopts){

    // display in big font, top line: "title (count)     arrow"
    // RSN - color
    printf("\e[J\e[16m\e[4m%s(%d)\e[24m\e[-1G\x84\n\e[16m", title, nopts);
}

static inline void
botline(const char *foot){
    if(!foot) return;
    // 5x8 font at bottom
    // RSN - color
    // RSN - no footer on small display
    printf("\e[13m\e[-1;0H%s\e[16m\e[1;0H", foot);
}

static const struct MenuOption *
domenu(const struct Menu *m){
    int nopt  = 0;
    int nopts = 0;

    // count options
    for(nopts=0; m->el[nopts].type; nopts++) {}

    topline(m->title, nopts);

    if( m->startval ) nopt = * m->startval;

    while(1){
        // display current choice, bottom line: "choice     arrow"
        printf("\e[0G%s\e[-1G%c\xB", m->el[nopt].text,
               (nopt==0 && nopts==1) ? ' ' :
               (nopt==0) ? '\x82' :
               (nopt==nopts-1) ? '\x80' : '\x85'
            );

        // get input
        int ch = get_input();

        switch(ch){
        case '?':
            // refresh screen
            topline(m->title, nopts);
            break;
        case '<':
            nopt --;
            if( nopt < 0 ){
                nopt = 0;
                beep(200, ivolume, 500000);
            }
            clear_line(-1);
            break;
        case '>':
            nopt ++;
            if( nopt > nopts - 1 ){
                nopt = nopts - 1;
                beep(200, ivolume, 500000);
            }
            clear_line(1);
            break;
        case '^':
            return (void*)-1;
        case '*':
            return (void*) & m->el[nopt];
        }
    }
}



void
menu(const struct Menu *m){
    Catchframe cf;
    const struct MenuOption *opt;
    const void *r;

    // catch ^C (or equiv)
    if(0){
    xyz:
        UNCATCH(cf);
        set_leds_rgb( 0xFF0000, 0xFF0000 );
        ui_resume();

        while(1){
            play(ivolume, "D+3>D-3>");
            read_imu_all();
            // make sure we are right side up
            if( accel_z() > 500 ) break;
        }
        set_leds_rgb( 0, 0 );
    }
    CATCHL(cf, MSG_CCHAR_0, xyz);

    while(1){
        if( !m ) return;

        opt = domenu(m);

        if( opt == (void*)-1 || opt == 0 ){
            r = (void*)opt;
        }else{
            switch(opt->type){
            case MTYP_EXIT:
                UNCATCH(cf);
                return;
            case MTYP_MENU:
                r = opt->action;
                break;
            case MTYP_FUNC:
                printf("\e[10m\e[J");
                r = ((void*(*)(int,const char**,void*))opt->action)(opt->argc, opt->argv, 0);
                // stay or back
                if( r ) r = (void*)-1;
                break;
            }
        }

        if( r == 0 ){
            // stay here
        }
        else if( r == (void*)-1 ){
            // go back
            m = m->prev;
        }else{
            // new menu
            m = (const struct Menu*)r;
        }
    }

    UNCATCH(cf);
}

/****************************************************************/

/* return user entered number */
int
menu_get_int(const char *prompt, const char *foot, int min, int max, int start){
    int nopt  = start;
    int nopts = max - min + 1;

    topline(prompt, nopts);
    botline(foot);

    while(1){
        // display current choice, bottom line: "choice     arrow"
        printf("\e[0G%d\e[99G%c\xB", nopt,
               (nopt==0 && nopts==1) ? ' ' :
               (nopt==0) ? '\x82' :
               (nopt==nopts-1) ? '\x80' : '\x85'
            );

        // get input
        int ch = get_input();

        switch(ch){
        case '?':
            topline(prompt, nopts);
            botline(foot);
            break;
        case '<':
            nopt --;
            if( nopt < 0 ){
                nopt = 0;
                beep(200, ivolume, 500000);
            }
            clear_line(-1);
            break;
        case '>':
            nopt ++;
            if( nopt > nopts - 1 ){
                nopt = nopts - 1;
                beep(200, ivolume, 500000);
            }
            clear_line(1);
            break;
        case '^':
            return -1;
        case '*':
            return nopt;
        }
    }
}

/* return user entered string */
int
menu_get_str(const char *prompt, const char *foot, int nopts, const char **argv, int start){
    int nopt  = start;

    topline(prompt, nopts);
    botline(foot);

    while(1){
        // display current choice, bottom line: "choice     arrow"
        printf("\e[0G%s\e[99G%c\xB", argv[nopt],
               (nopt==0 && nopts==1) ? ' ' :
               (nopt==0) ? '\x82' :
               (nopt==nopts-1) ? '\x80' : '\x85'
            );

        // get input
        int ch = get_input();

        switch(ch){
        case '?':
            topline(prompt, nopts);
            botline(foot);
            break;
        case '<':
            nopt --;
            if( nopt < 0 ){
                nopt = 0;
                beep(200, ivolume, 500000);
            }
            clear_line(-1);
            break;
        case '>':
            nopt ++;
            if( nopt > nopts - 1 ){
                nopt = nopts - 1;
                beep(200, ivolume, 500000);
            }
            clear_line(1);
            break;
        case '^':
            return -1;
        case '*':
            return nopt;
        }
    }
}


/* pick from list of shorts, return index */
int
menu_get_pshort(const char *prompt, const char *foot, int nopts, const short *values, int start){
    int nopt  = start;

    topline(prompt, nopts);
    botline(foot);

    while(1){
        // display current choice, bottom line: "choice     arrow"
        printf("\e[0G%d\e[99G%c\xB", values[nopt],
               (nopt==0 && nopts==1) ? ' ' :
               (nopt==0) ? '\x82' :
               (nopt==nopts-1) ? '\x80' : '\x85'
            );

        // get input
        int ch = get_input();

        switch(ch){
        case '?':
            topline(prompt, nopts);
            botline(foot);
            break;
        case '<':
            nopt --;
            if( nopt < 0 ){
                nopt = 0;
                beep(200, ivolume, 500000);
            }
            clear_line(-1);
            break;
        case '>':
            nopt ++;
            if( nopt > nopts - 1 ){
                nopt = nopts - 1;
                beep(200, ivolume, 500000);
            }
            clear_line(1);
            break;
        case '^':
            return -1;
        case '*':
            return nopt;
        }
    }
}
