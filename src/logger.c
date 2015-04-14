/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-07 19:26 (EDT)
  Function: data logging, etc

*/


#include <conf.h>
#include <proc.h>
#include <stm32.h>
#include <adc.h>
#include <pwm.h>
#include <userint.h>

#include "util.h"


#define LOGFMT_CSV	0
#define LOGFMT_JSON	1
#define LOGFMT_TXT	2
#define LOGFMT_RAW	3
#define LOGFMT_SLN	4
#define LOGFMT_ALAW	5
#define LOGFMT_XML	6

// double buffer. sd driver runs best at 8k writes
#define WRITESIZE	8192
#define BUFSIZE		(4 * WRITESIZE)

#define LONGTIME	5000000		// 5 sec
#define SLOPTIME	1

// lowpower = 1 => try to log+power off, 0 => stay on
DEFVAR(int,        logger_lowpower, 1,            UV_TYPE_UL    | UV_TYPE_CONFIG, "should we optimize for low power")
DEFVAR(uv_str16_t, logger_script,  "logger.cnf",  UV_TYPE_STR16 | UV_TYPE_CONFIG, "logger control script")
DEFVAR(uv_str16_t, logger_logfile, "logfile.log", UV_TYPE_STR16 | UV_TYPE_CONFIG, "logger output file")
// debugging
DEFVAR(int, logger_beep_acq, 0, UV_TYPE_UL, "beep")
DEFVAR(int, logger_beep_buf, 0, UV_TYPE_UL, "beep")

static utime_t  log_usec      = 0;
static char     log_usec_txt[32];
static int      log_fmt       = 0;
static void     (*log_output)(void) = 0;
static uint32_t chkval        = 0; // the script needs these values
static uint32_t logval        = 0; // output these values
static proc_t   logproc_pid   = 0;
static bool     logproc_close = 0;
static bool     logproc_pause = 0;
static bool     use_timer     = 0;
static int      log_debug  = 0;
static short    values[32];


static char    datbuf[BUFSIZE];
static int     logpos          = 0;
static int     savepos         = 0;
static int     logend          = 0;
static proc_t  bufproc_pid     = 0;
static bool    bufproc_close   = 0;
static bool    bufproc_pause   = 0;
static bool    bufproc_flush   = 0;
static bool    bufproc_rotate  = 0;
static FILE   *logfd           = 0;
static bool    overflow_warned = 0;

static void logproc(void);
static void log_script(void);
static void logbuf_save(void);
static void log_out_csv(void);
static void log_out_txt(void);
static void log_out_json(void);
static void log_out_raw(void);
static void log_out_sln(void);
static void log_out_alaw(void);
static void log_out_xml(void);
static void set_logger_format(void);
static void logger_wakeup(void);

static const char * const vars[] = {
    "AX", "AY", "AZ",
    "MX", "MY", "MZ",
    "GX", "GY", "GZ",
    "TP", "U1"
};



//****************************************************************

static inline void
_lock(void){
    irq_disable();
}

static inline void
_unlock(void){
    irq_enable();
}

//****************************************************************

// acquire the needed values
static inline void
log_acquire(void){
    uint32_t acqval = chkval | logval;
    int8_t i;

    for(i=0; i<32; i++){
        if( acqval & (1<<i) ) values[i] = getpini( i );
    }
}


// log once + done
static void
log_one(void){
    log_acquire();
    log_script();
    if( log_output ) log_output();
}

static void
logger_stop(void){

    logbuf_close();
    logproc_close = 1;

    while( logproc_pid ){
        sigunblock( logproc_pid );
        usleep(10000);
    }

    logproc_close = 0;
    timer_init( 0 );
}

static void
logger_init_timer(void){

    if( log_usec < 1000000 ){
        timer_init( log_usec );
        use_timer = 1;
    }else{
        timer_init( 0 );
        use_timer = 0;
    }
}

void
logger_init(void){

    // already running?
    if( logproc_pid ) return;

    if( !logger_config() ) return;

    RTC->BKP2R = 0;

    if( log_usec >= LONGTIME && logger_lowpower ){
        logger_wakeup();
        // not reached
    }

    // start 2 procs: one to acquire data, one to write to disk
    logbuf_open( logger_logfile );
    logproc_pid = start_proc( 1024, logproc, "log/acq");

    logger_init_timer();
}

DEFUN(logger_start, "start logger")
{
    logger_init();
    return 0;
}

DEFUN(logger_stop, "stop logger")
{
    logger_stop();
}

DEFUN(logger_reload, "reload logger config")
{
    if( !logger_config() )
        logger_stop();

    return 0;
}

DEFUN(logger_rotate, "rotate logger logfile")
{
    bufproc_rotate = 1;
    return 0;
}

static void
goto_sleep(uint32_t sec){

    if( sec > 3600 * 13 ){
        sec = 3600 * 12;
    }

    set_rtc_wakeup( sec );
    RTC->BKP1R = logger_wakeup;
    shutdown();
    // not reached
}

static void
logger_wakeup(void){
    uint32_t now = get_time() / 1000000;
    uint32_t sec = log_usec   / 1000000;

    if( logger_config() ){
        // fast sample rate? skip, and continue with normal startup
        if( log_usec < LONGTIME ) return;
        if( ! logger_lowpower )   return;

        if( RTC->BKP2R && now + SLOPTIME < RTC->BKP2R ){
            // back to sleep
            goto_sleep( RTC->BKP2R - now );
        }
        log_one();
        logbuf_save();
    }

    // set alarm + power off
    // rtc maxes out at ~36 hours,
    // store the time in R2

    if( RTC->BKP2R < now && RTC->BKP2R > now - sec ){
        // prevent slip due to startup overhead
        RTC->BKP2R = RTC->BKP2R + sec;
        sec = RTC->BKP2R - now;
    }else{
        RTC->BKP2R = now + sec;
    }

    goto_sleep( sec );
}

int
logger_config(void){

    logproc_pause = 1;

    // RSN - load file

    set_logger_format();
    logproc_pause = 0;

    // are we good to go?
    if( ! logger_logfile[0] ) return 0;
    if( ! log_usec ) return 0;
    if( ! logval )   return 0;

    return 1;
}

static void
logproc(void){
    short x = 0;

    proc_detach(0);
    currproc->flags |= PRF_REALTIME | PRF_SIGWAKES;
    currproc->prio   = 0;	// highest priority

    utime_t t0 = get_time();

    while( !logproc_close ){

        if( logproc_pause ){
            usleep( 100000 );
            continue;
        }

        if(logger_beep_acq) play(4, "a7>>");

        log_debug = 1;
        log_one();
        log_debug = 2;
        if( use_timer ){
            log_debug = 3;
            timer_wait();
            log_debug = 4;
        }else{
            utime_t t1 = get_time();
            t0 += log_usec;
            utime_t usec = t0 - t1;

            // NB: might be > 32 bits, loop+sleep
            int l = (usec >> 32) + 1;
            usec &= 0xFFFFFFFF;

            log_debug = 5;
            for(; l>0; l-- ){
                usleep( usec );
                if( logproc_close ) break;
            }
            log_debug = 6;
        }
    }

    logproc_close = 0;
    logproc_pid   = 0;
}

//****************************************************************

static int
error(const char *msg){
    fprintf(STDERR, msg);
    return 0;
}

static void
set_logger_format(void){

    switch(log_fmt){
    case LOGFMT_CSV:	log_output = log_out_csv;	break;
    case LOGFMT_JSON:	log_output = log_out_json;	break;
    case LOGFMT_TXT:	log_output = log_out_txt;	break;
    case LOGFMT_RAW:	log_output = log_out_raw;	break;
    case LOGFMT_SLN:	log_output = log_out_sln;	break;
    case LOGFMT_ALAW:	log_output = log_out_alaw;	break;
    case LOGFMT_XML:	log_output = log_out_xml;	break;

    default:		log_output = log_out_raw; 	break;
    }
}

DEFUN(logger_format, "set log file format")
{
    if( argc < 2 ) return error("logger_fmt csv|txt|json|raw|sln|alaw|xml\n");

    if( !strcmp(argv[1], "raw") ){
        log_fmt    = LOGFMT_RAW;
    }else if( !strcmp(argv[1], "json") ){
        log_fmt = LOGFMT_JSON;
    }else if( !strcmp(argv[1], "txt") ){
        log_fmt = LOGFMT_TXT;
    }else if( !strcmp(argv[1], "csv") ){
        log_fmt = LOGFMT_CSV;
    }else if( !strcmp(argv[1], "xml") ){
        log_fmt = LOGFMT_XML;
    }else if( !strcmp(argv[1], "sln") ){
        log_fmt = LOGFMT_SLN;
    }else if( !strcmp(argv[1], "alaw") ){
        log_fmt = LOGFMT_ALAW;
    }else
        return error("invalid format: csv|txt|json|raw|alaw|xml\n");

    return 0;
}

// with optional units eg 100m (no space)
// u - usec
// m - millisec
// M - minutes
// H - hours
// D - days
DEFUN(logger_rate, "set logging rate")
{
    if( argc < 2 ) return error("specify seconds\n");
    utime_t usec = atoi( argv[1] );

    // find units
    char i=0, u=0;
    while( argv[1][i] ){
        if( !isdigit(argv[1][i]) ) u = argv[1][i];
        i++;
    }
    switch(u){
    case 'm':	usec *= 1000;	break;
    case 'u':	break;

        // fall all the way through the chain:
    case 'D':	usec *= 24;
    case 'H': 	usec *= 60;
    case 'M':	usec *= 60;
    default:
        usec *= 1000000; break;
    }

    strncpy(log_usec_txt, argv[1], sizeof(log_usec_txt));

    logproc_pause = 1;
    log_usec = usec;
    if( logproc_pid ) logger_init_timer();
    logproc_pause = 0;

    return 0;
}

// +A0 - enable logging
// -A0 - disable
DEFUN(logger_values, "specify which values to log")
{
    short i, p, e;

    for(i=1; i<argc; i++){
        p = -1;
        e = 1;

        if( argv[i][0] == '-' ){
            e = 0;
            p = find_pin(argv[i] + 1);
        }
        else if( argv[i][0] == '+' ){
            p = find_pin(argv[i] + 1);
        }
        else{
            p = find_pin(argv[i]);
        }

        if( p == -1 )
            fprintf(STDERR, "invalid pin '%s'\n", argv[i]);
        else if( e )
            logval |= 1<<p;
        else
            logval &= ~(1<<p);

    }
    return 0;
}

DEFCONFUNC(logconf, f)
{
    short i;

    // logvals
    // logusec
    // fmt

    switch(log_fmt){
    case LOGFMT_CSV:	fprintf(f, "logger_format csv\n");	break;
    case LOGFMT_JSON:	fprintf(f, "logger_format json\n");	break;
    case LOGFMT_TXT:	fprintf(f, "logger_format txt\n");	break;
    case LOGFMT_RAW:	fprintf(f, "logger_format raw\n");	break;
    case LOGFMT_SLN:	fprintf(f, "logger_format sln\n");	break;
    case LOGFMT_ALAW:	fprintf(f, "logger_format alaw\n");	break;
    case LOGFMT_XML:	fprintf(f, "logger_format xml\n");	break;
    }

    if( logval ){
        fprintf(f, "logger_values");
        for(i=0; i<32; i++)
            if( logval & (1<<i) ) fprintf(f, " %s", getpinname(i));
        fprintf(f, "\n");
    }

    if( log_usec ){
        fprintf(f, "logger_rate %s\n", log_usec_txt);
    }

}

static void
log_script(void){

}

//****************************************************************

/* add to datbuf:
   add at datbuf + logpos
   inc logpos
*/


static void
_save_buf(int endpos){

    // save from [savepos .. endpos)
    // possibly with wraparound

    // kprintf("save %d..%d\n", savepos, endpos);

    if( savepos < endpos ){
        fwrite(logfd, datbuf + savepos, endpos - savepos);
    }else{
        fwrite(logfd, datbuf + savepos, BUFSIZE - savepos);
        fwrite(logfd, datbuf, endpos);
    }

    _lock();
    savepos = endpos;
    if( savepos >= BUFSIZE ) savepos -= BUFSIZE;
    _unlock();

    fflush(logfd);
    if(logger_beep_buf) play(4, "a-7>>");
}

static void
_save_all(void){
    int c = 4;

    while( logpos != savepos ){
        _save_buf(logpos);
        if( !--c ) break;
    }
}

static inline void
_buf_rotate(void){

    bufproc_rotate = 0;

    _save_all();
    fclose(logfd);

    logfd = fopen(logger_logfile, "w");
    if( !logfd ){
        kprintf("cannot open '%s'\n", logger_logfile);
    }

    overflow_warned = 0;
    set_logger_format();
}

static void
_buf_proc(void){
    int slp;

    proc_detach(0);
    overflow_warned = 0;

    while( !bufproc_close ){
        usleep(1000);
        if( bufproc_pause ) continue;

        if( bufproc_rotate ){
            _buf_rotate();
            if( !logfd ) break;
        }

        _lock();
        slp = logpos;
        _unlock();

        if( bufproc_flush ){
            // save entire buffer
            if( slp != savepos ) _save_buf(slp);
            bufproc_flush = 0;
        }else{
            int endpos = (savepos + WRITESIZE) & ~(WRITESIZE - 1);
            if( slp >= endpos || slp < savepos ) _save_buf( endpos );
        }
    }

    if( logfd ){
        _save_all();
        fclose(logfd);
    }

    bufproc_close = 0;
    bufproc_pid   = 0;
}

void
logbuf_reset(void){
    logpos  = 0;
    savepos = 0;
}

void
logbuf_flush(void){
    bufproc_flush = 1;
}

int
logbuf_open(const char *file){
    if( bufproc_pid ) return 0;

    logfd = fopen(file, "a");
    if( !logfd ) return 0;

    logbuf_reset();
    bufproc_close = 0;
    bufproc_flush = 0;
    bufproc_pid   = start_proc( 1024, _buf_proc, "log/buf");
    yield();

    return 1;
}

int
logbuf_close(void){
    if( !bufproc_pid ) return 0;

    proc_t pid = bufproc_pid;
    bufproc_close = 1;

    return 1;
}

static void
logbuf_save(void){

    if( bufproc_pid ) return;

    logfd = fopen(logger_logfile, "a");
    if( !logfd ) return;
    _save_all();
    fclose(logfd);

}

static void
log_append( const char *data, int len ){

    _lock();
    while( len-- ){
        // buffer full?
        if( savepos ? (logpos == savepos - 1) : (logpos == BUFSIZE - 1) ){
            if( !overflow_warned ){
                kprintf("logger buffer overflow\n");
                overflow_warned = 1;
            }
            break;
        }

        datbuf[ logpos ++ ] = *data ++;
        if( logpos == BUFSIZE ) logpos = 0;
    }
    _unlock();
}

/****************************************************************/

static void
log_out_raw(void){
    short i;

    for(i=0; i<32; i++){
        if( logval & (1<<i) ) log_append( (char*)& values[i], 2 );
    }
}

static void log_out_sln(void){
    short i;

    for(i=0; i<32; i++){
        if( logval & (1<<i) ){
            short v = values[i] - 4096;
            log_append( (char*)&v, 2 );
        }
    }
}

// unsigned 12 bit compressed into 8 bit
// alaw-like / 3e5m floating point encoding
static inline int
alaw_encode(int v){

    if( v <= 0x1F ) return v;
    if( v > 0xFFF ) v = 0xFFF;

    int e = 1;
    while( v <= 0x3F ){
        v >>= 1;
        e ++;
    }

    v &= 0x1F;	// discard redundant 1-bit
    v |= e<<5;

    return v;
}

static void
log_out_alaw(void){
    short i;

    for(i=0; i<32; i++){
        if( logval & (1<<i) ){
            int8_t v = alaw_encode( values[i] );
            log_append( (char*)&v, 1 );
        }
    }
}

static void
fappend(void *x, int c){
    char ch = c;
    log_append( &ch, 1 );
}

static void
log_out_csv(void){
    short i;

    fncprintf(fappend, 0, "%#T", get_hrtime());
    for(i=0; i<32; i++){
        if( logval & (1<<i) ){
            fncprintf(fappend, 0, ", %d", values[i]);
        }
    }
    log_append("\n", 1);
}

static void
log_out_json(void){
    short i;

    fncprintf(fappend, 0, "{\"time\": \"%#T\"", get_hrtime());
    for(i=0; i<32; i++){
        if( logval & (1<<i) ){
            fncprintf(fappend, 0, ", \"%s\": %d", getpinname(i), values[i]);
        }
    }
    log_append("}\n", 2);
}

static void
log_out_txt(void){
    short i;

    fncprintf(fappend, 0, "time:\t%#T\n", get_hrtime());
    for(i=0; i<32; i++){
        if( logval & (1<<i) ){
            fncprintf(fappend, 0, "%s:\t%d\n", getpinname(i), values[i]);
        }
    }
    log_append("\n", 1);
}

// mostly just to generate lots of data for testing
static void
log_out_xml(void){
    short i;

    fncprintf(fappend, 0, "<acquisition>\n"
              "  <platform>AMIKETO</platform>\n"
              "  <version>1.0</version>\n"
              "  <timestamp>%#T</timestamp>\n  <dataset>\n", get_hrtime());

    for(i=0; i<32; i++){
        if( logval & (1<<i) ){
            fncprintf(fappend, 0, "    <datapoint><pin>%s</pin><value>%d</value></datapoint>\n", getpinname(i), values[i]);
        }
    }
    fncprintf(fappend, 0, "  </dataset>\n</acquisition>\n");
}

#ifdef KTESTING
DEFUN(logdump, "")
{
    printf("logpos %d, savepos %d, dbg %d\n", logpos, savepos, log_debug);
    return 0;
}
#endif


/****************************************************************/

/*
  config script:

  if A0 > 123 & B4 | A7 < 12 {
  log #	- 0 = don't log, 1 = log, 2 = log this + next sample, ... (store in rtc reg)

  }

  if A2 & A3 > 100 {
      setpin A6 127
  }

  if A5 {
      rm file.csv
  }

  if expr then
    x x x
  else
    y y y
  endif

  [ABCDE][0-9]	- pins
  [AGM][XYZ]	- imu
  UV		- user variable (RTC reg)
*/

