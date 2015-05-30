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

#include "defproto.h"
#include "util.h"


#define LOGFMT_CSV	0
#define LOGFMT_JSON	1
#define LOGFMT_TXT	2
#define LOGFMT_RAW	3
#define LOGFMT_SLN	4
#define LOGFMT_ALAW	5
#define LOGFMT_XLAW	6	// unsigned alaw variant
#define LOGFMT_XML	7
#define LOGFMT_BITS	8	// raw packed bits (digital inputs only)
#define LOGFMT_TCA	9	// tagged compressed alaw

// sd driver runs best at 8k writes
#define WRITESIZE	8192
#define BUFSIZE		(4 * WRITESIZE)

#define LONGTIME	5000000		// 5 sec
#define SLOPTIME	1		// the RTC wakeup has a 1 second resolution

// lowpower = 1 => try to log+power off, 0 => stay on
DEFVAR(int,        logger_lowpower, 1,            UV_TYPE_UL    | UV_TYPE_CONFIG, "should we optimize for low power")
DEFVAR(uv_str16_t, logger_script,  "",            UV_TYPE_STR16 | UV_TYPE_CONFIG, "logger control script")
DEFVAR(uv_str16_t, logger_logfile, "logfile.log", UV_TYPE_STR16 | UV_TYPE_CONFIG, "logger output file")
// number of samples to save. this is saved in RTC/R3
DEFVAR(int,        logger_count,    1,            UV_TYPE_UL, "logger save count")
// debugging
DEFVAR(int, logger_beep_acq, 0, UV_TYPE_UL, "beep")
DEFVAR(int, logger_beep_buf, 0, UV_TYPE_UL, "beep")



// for the data acquisition process
static utime_t  log_usec            = 0;	// current sample period
static char     log_usec_txt[32];		// sample rate in text format
static int      log_fmt             = 0;	// log file format
static void     (*log_output)(void) = 0;
static proc_t   logproc_pid         = 0;
static bool     logproc_close       = 0;
static bool     logproc_pause       = 0;
static bool     use_timer           = 0;	// are we using the timer or os scheduler

uint32_t log_chkval                 = 0; 	// the script needs these values
uint32_t log_logval                 = 0; 	// output these values
short    log_values[32];			// adc values

static int     overrun_count   = 0;
static bool    overrun_warned  = 0;
static bool    overflow_warned = 0;

// for the disk write process
char    datbuf[BUFSIZE];			// output buffer
static int     logpos          = 0;		// current end position
static int     savepos         = 0;		// current start
static proc_t  bufproc_pid     = 0;
static bool    bufproc_close   = 0;
static bool    bufproc_pause   = 0;
static bool    bufproc_flush   = 0;
static bool    bufproc_rotate  = 0;
static FILE   *logfd           = 0;

extern u_char progmem[];


static void logproc(void);
static void logbuf_save(void);
static void log_out_csv(void);
static void log_out_txt(void);
static void log_out_json(void);
static void log_out_raw(void);
static void log_out_sln(void);
static void log_out_alaw(void);
static void log_out_xlaw(void);
static void log_out_xml(void);
static void log_out_bits(void);
static void log_out_tca(void);
static void set_logger_format(void);
static void logger_wakeup(void);

extern void compile_script(void);
extern void run_bytecode(void);

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

// acquire the needed log_values
static inline void
log_acquire(void){
    uint32_t acqval = log_chkval | log_logval;

    getpins_all(acqval, log_values);
}


// log once + done
void
log_one(void){

    log_acquire();

    if( progmem[0] ){
        if( logger_count > 0 ) logger_count --;
        run_bytecode();
    }else{
        // no script? always output
        logger_count = 1;
    }
    if( log_output && logger_count > 0 ) log_output();
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
    set_blinky(0);
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

extern int ivolume;
extern const struct Menu guilogon, guilogoff;

DEFUN(logger_menu, 0)
{
    if( logproc_pid ){
        menu( &guilogon );
    }else if( log_usec ){
        menu( &guilogoff );
    }else{
        printf("not configured\n");
        play(ivolume, "d4>>g4->g4->g4->");
        usleep(500000);
    }

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
        logger_count = RTC->BKP3R;
        log_one();
        logbuf_save();
        RTC->BKP3R = logger_count;
    }else{
        // why are we awake? power down.
        goto_sleep(0);
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

    compile_script();
    set_logger_format();

    logproc_pause = 0;

    // are we good to go?
    if( ! logger_logfile[0] ) return 0;
    if( ! log_usec ) return 0;
    if( ! log_logval )   return 0;

    return 1;
}

static utime_t stats_start = 0;
static int stats_count = 0;

DEFUN(logger_stats, "stats")
{
    int dt = get_hrtime() - stats_start;
    int rate = 1000 * stats_count / (dt / 1000);
    printf("dt %d, ct: %d, rate: %d\n", dt, stats_count, rate);
    return 0;
}

static void
logproc(void){
    short x = 0;

    proc_detach(0);
    currproc->flags |= PRF_REALTIME | PRF_SIGWAKES;
    currproc->prio   = 0;	// highest priority

    set_blinky(0);
    overrun_count  = 0;
    overrun_warned = 0;
    utime_t t0 = get_hrtime();
    stats_start = t0;

    while( !logproc_close ){

        if( logproc_pause ){
            usleep( 100000 );
            continue;
        }

        if(logger_beep_acq) play(4, "a7>>");

        log_one();
        if( use_timer ){
#if 0
            utime_t t1 = get_hrtime();
            utime_t usec = t1 - t0;
            t0 = t1;
            if( usec >= 2*log_usec ){
                if( ++overrun_count > 5 && !overrun_warned ){
                    kprintf("logger sample rate too fast %d\n", (int)usec);
                    set_blinky(4);
                    overrun_warned = 1;
                }
            }else
                overrun_count = 0;
#endif
#if 1
            stats_count ++;
            utime_t t1 = get_hrtime();
            if( t1 - stats_start > 10000000 ){
                stats_count = 0;
                stats_start = t1;
            }
#endif

            timer_wait();
        }else{
            utime_t t1 = get_time();
            t0 += log_usec;
            utime_t usec = t0 - t1;

            // NB: might be > 32 bits, loop+sleep
            int l = (usec >> 32) + 1;
            usec &= 0xFFFFFFFF;

            for(; l>0; l-- ){
                usleep( usec );
                if( logproc_close ) break;
            }
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

static const struct {
    const char *name;
    int value;
    void (*func)(void);
} _log_formats[] = {
    { "csv",  LOGFMT_CSV,  log_out_csv  },
    { "json", LOGFMT_JSON, log_out_json },
    { "txt",  LOGFMT_TXT,  log_out_txt  },
    { "raw",  LOGFMT_RAW,  log_out_raw  },
    { "sln",  LOGFMT_SLN,  log_out_sln  },
    { "alaw", LOGFMT_ALAW, log_out_alaw },
    { "xlaw", LOGFMT_XLAW, log_out_xlaw },
    { "xml",  LOGFMT_XML,  log_out_xml  },
    { "bits", LOGFMT_BITS, log_out_bits },
    { "tca",  LOGFMT_TCA,  log_out_tca },
};


static void
set_logger_format(void){
    short i;

    for(i=0; i<ELEMENTSIN(_log_formats); i++){
        if( _log_formats[i].value == log_fmt ){
            log_output = _log_formats[i].func;
            return;
        }
    }

    log_output = log_out_raw;
}

DEFUN(logger_format, "set log file format")
{
    short i;

    if( argc >= 2 ){
        for(i=0; i<ELEMENTSIN(_log_formats); i++){
            if( !strcmp(argv[1], _log_formats[i].name) ){
                log_fmt = _log_formats[i].value;
                return 0;
            }
        }
    }

    fprintf(STDERR, "%s:", argv[0]);
    for(i=0; i<ELEMENTSIN(_log_formats); i++){
        fprintf(STDERR, "%c%s", (i?'|':' '), _log_formats[i].name);
    }
    fprintf(STDERR, "\n");

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
            log_logval |= 1<<p;
        else
            log_logval &= ~(1<<p);

    }
    return 0;
}

DEFCONFUNC(logconf, f)
{
    short i;

    // log_logvals
    // logusec
    // fmt

    for(i=0; i<ELEMENTSIN(_log_formats); i++){
        if( _log_formats[i].value == log_fmt ){
            fprintf(f, "logger_format %s\n", _log_formats[i].name);
            break;
        }
    }

    if( log_logval ){
        fprintf(f, "logger_values");
        for(i=0; i<32; i++)
            if( log_logval & (1<<i) ) fprintf(f, " %s", getpinname(i));
        fprintf(f, "\n");
    }

    if( log_usec ){
        fprintf(f, "logger_rate %s\n", log_usec_txt);
    }

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
    set_blinky(4);
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
                set_blinky(4);
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
        if( log_logval & (1<<i) ) log_append( (char*)& log_values[i], 2 );
    }
}

static void log_out_sln(void){
    short i;

    for(i=0; i<32; i++){
        if( log_logval & (1<<i) ){
            short v = log_values[i] - 2048;
            log_append( (char*)&v, 2 );
        }
    }
}

static void
log_out_bits(void){
    short i, v=0;

    for(i=0; i<32; i++){
        if( log_logval & (1<<i) && log_values[i] ) v |= (1<<i);
    }

    log_append( (char*)& v, 2 );
}

// unsigned 12 bit compressed into 8 bit
// alaw-like / 3e5m floating point encoding
static inline int
xlaw_encode(int v){

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
log_out_xlaw(void){
    short i;

    for(i=0; i<32; i++){
        if( log_logval & (1<<i) ){
            int8_t v = xlaw_encode( log_values[i] );
            log_append( (char*)&v, 1 );
        }
    }
}

static inline int
alaw_encode(int v){
    char sign = 0x80;	// 1 => positive

    if( v<0 ){
        sign = 0;
        v = - v;
    }

    if( v <= 0x0F ) return (v | sign) ^ 0x55;
    if( v > 0x7FF ) v = 0x7FF;

    int e = 1;
    while( v <= 0x1F ){
        v >>= 1;
        e ++;
    }

    v &= 0x0F;	// discard redundant 1-bit
    v |= e<<4;
    v |= sign;
    v ^= 0x55;

    return v;
}

static void
log_out_alaw(void){
    short i;

    for(i=0; i<32; i++){
        if( log_logval & (1<<i) ){
            int8_t v = alaw_encode( log_values[i] - 2048 );
            log_append( (char*)&v, 1 );
        }
    }
}


#define QUIETLVL	410
#define QUIETCNT	 20

// alaw value 0x55 (negative 0) is not used as a value, so we usurp it as an escape for tagged metadata
// <55><tag><data>
// tag 00     => 64bit usec timestamp
// tag 01..FF => future use

static void
log_out_tca(void){
    short i;
    static utime_t tnext = 0;
    static short qcount  = 0;
    utime_t now = get_hrtime();


    // detect + remove silence
    qcount ++;
    for(i=0; i<32; i++){
        if( log_logval & (1<<i) ){
            short v = log_values[i] - 2048;
            if( v > QUIETLVL || v <-QUIETLVL ) qcount = 0;
        }
    }

    if( qcount >= QUIETCNT ){
        tnext = 0;
        return;
    }

    // add timestamp
    if( now >= tnext ){
        i = 0x0055;		// little-endian
        log_append( (char*)&i,   2);
        log_append( (char*)&now, 8);
        tnext = now + 1000000;	// no more than 1 per second
    }

    // add values
    for(i=0; i<32; i++){
        if( log_logval & (1<<i) ){
            int8_t v = alaw_encode( log_values[i] - 2048 );
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
        if( log_logval & (1<<i) ){
            fncprintf(fappend, 0, ", %d", log_values[i]);
        }
    }
    log_append("\n", 1);
}

static void
log_out_json(void){
    short i;

    fncprintf(fappend, 0, "{\"time\": \"%#T\"", get_hrtime());
    for(i=0; i<32; i++){
        if( log_logval & (1<<i) ){
            fncprintf(fappend, 0, ", \"%s\": %d", getpinname(i), log_values[i]);
        }
    }
    log_append("}\n", 2);
}

static void
log_out_txt(void){
    short i;

    fncprintf(fappend, 0, "time:\t%#T\n", get_hrtime());
    for(i=0; i<32; i++){
        if( log_logval & (1<<i) ){
            fncprintf(fappend, 0, "%s:\t%d\n", getpinname(i), log_values[i]);
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
        if( log_logval & (1<<i) ){
            fncprintf(fappend, 0, "    <datapoint><pin>%s</pin><value>%d</value></datapoint>\n", getpinname(i), log_values[i]);
        }
    }
    fncprintf(fappend, 0, "  </dataset>\n</acquisition>\n");
}

#ifdef KTESTING
DEFUN(logdump, "")
{
    printf("logpos %d, savepos %d\n", logpos, savepos);
    return 0;
}
#endif


