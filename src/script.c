/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-12 16:02 (EDT)
  Function: logger control script

*/

#ifdef STANDALONE
#include <stdio.h>
#include <sys/types.h>
#define STDERR stderr
#define ELEMENTSIN(T) (sizeof(T)/sizeof(T[0]))
char logger_script[32];
uint32_t log_chkval;
uint32_t log_logval;
short log_values[32];

#else
#include <conf.h>
#include <proc.h>
#include <stm32.h>
#include <adc.h>
#include <pwm.h>
#include <userint.h>

#include "defproto.h"
#endif

#include "util.h"
#include "script.h"

#define ARGLEN 		8		// maximum args in script commands
#define ENDOFILE	-1

typedef unsigned char uchar;
typedef   signed char schar;

uchar   progmem[1024];			// bytecode
static int     proglen         = 0;	// length (for debugging only)
static short   stack[16];		// rpn value stack

extern uint32_t log_chkval;
extern uint32_t log_logval;
extern short    log_values[32];


struct Operator {
    const char *name;
    char ispfx;	// is this a prefix of another op? (do we need to read more chars?)
    char argc;	// how many args does this use?
    char code;
};
const struct Operator ops[] = {
    { "",    0, 0, LSTOK_VAR },

    { "!",   1, 1, LSTOK_NOT },
    { "~",   0, 1, LSTOK_INV },
    { "*",   0, 2, LSTOK_MULT },
    { "/",   0, 2, LSTOK_DIV },
    { "%",   0, 2, LSTOK_MOD },
    { "+",   0, 2, LSTOK_PLUS },
    { "-",   0, 2, LSTOK_MINUS },
    { "<<",  0, 2, LSTOK_SHL },
    { ">>",  0, 2, LSTOK_SHR },
    { "&",   1, 2, LSTOK_BAND },
    { "|",   1, 2, LSTOK_BOR },
    { "^",   0, 2, LSTOK_BXOR },
    { ">",   1, 2, LSTOK_GT },
    { "<",   1, 2, LSTOK_LT },
    { ">=",  0, 2, LSTOK_GE },
    { "<=",  0, 2, LSTOK_LE },
    { "==",  0, 2, LSTOK_EQ },
    { "!=",  0, 2, LSTOK_NEQ },
    { "&&",  0, 2, LSTOK_LAND },
    { "||",  0, 2, LSTOK_LOR },
    { "not", 0, 1, LSTOK_NOT },
    { "and", 0, 2, LSTOK_LAND },
    { "or",  0, 2, LSTOK_LOR },
    { "(",   0, 0, LSTOK_LPAREN },
    { ")",   0, 0, LSTOK_RPAREN },

    { "if",    0, 0, LSTOK_IF },
    { "then",  0, 0, LSTOK_THEN },

};

static int
find_op(const char *name){
    short i;

    for(i=0; i<ELEMENTSIN(ops); i++){
        if( !strcmp(name, ops[i].name) ) return i;
    }
    return -1;
}

static int
next_char(FILE* f){

    int c = fgetc(f);
    if( c == -1 || c == 255 ) return ENDOFILE;
    if( c == '#' ){
        // eat comment until end of line
        while(1){
            c = fgetc(f);
            if( c == -1 || c == 255 ) return ENDOFILE;
            if( c == '\n' )  return c;
        }
    }
    return c;
}

// return: -1 => eof|error, 0 => var, # => index in ops[]
static int
read_next_token(FILE *f, char *buf, int buflen, uchar *tok){
    short i=0, c;
    schar t=0, ct, mb=-1;

    buf[0] = 0;
    *tok = -1;

    while(1){
        c = next_char(f);
        ct = isalnum(c) ? 1 : 0;

        if( c == ENDOFILE ) return -1;

        if( isspace(c) ){
            if( i ) break;
            continue;
        }

        if( i && t != ct ){
            ungetc(c, f);
            break;
        }

        if( !i ) t = ct;
        if( i == buflen - 1 ) return -1;
        buf[i++] = c;
        buf[i]   = 0;

        if( !ct && mb==-1 ){
            mb = find_op(buf);
            if( mb != -1 && !ops[mb].ispfx ) break;	// one char punct
        }
        if( i > 1 && mb != -1 && find_op(buf) == -1 ){
            // eg. <! - put the ! back, return <
            ungetc(c, f);
            buf[--i] = 0;
            break;
        }
    }

    i = find_op(buf);

    if( i >= 0 )
        *tok = ops[i].code;

    if( i == -1 ){
        if( isdigit(buf[0]) )
            *tok = LSTOK_CONST;
        else
            *tok = LSTOK_VAR;
        i = 0;
    }

    // printf(">%d %s [%d %s]\n", i, buf, *tok, ops[i].name);
    return i;
}

// 0 => ok, else error
static const char *
compile_expr(FILE *f, char *buf, int buflen, short *ppc){
    short sp=0;
    short pc=*ppc;
    short check=0;
    uchar tok, tkn;
    short a;

    while(1){
        // read expr in algebraic format, compile into rpn bytecode
        // http://en.wikipedia.org/wiki/Shunting-yard_algorithm
        tkn = read_next_token(f, buf, buflen, &tok);
        if( tok == ENDOFILE )      	return "eof";
        if( tok == LSTOK_THEN ) 	break;		// done
        if( tok == LSTOK_IF )   	return "extra 'if'";
        if( tok == LSTOK_VAR ){
            if( pc >= ELEMENTSIN(progmem) - 3 ) return "program too big";
            progmem[pc++] = LSTOK_VAR;
            tkn = find_pin(buf);	// RSN - other vars
            if( tkn == -1 ) 		return "invalid var";
            progmem[pc++] = tkn;
            log_chkval |= 1<<tkn;
            check ++;

        }else if( tok == LSTOK_CONST ){
            if( pc >= ELEMENTSIN(progmem) - 4 ) return "program too big";
            progmem[pc++] = LSTOK_CONST;
            a = atoi(buf);
            progmem[pc++] = a;
            progmem[pc++] = a>>8;
            check ++;

        }else if( tok == LSTOK_LPAREN ){
            if( sp >= ELEMENTSIN(stack) ) return "stack overflow";
            stack[sp++] = tok;
        }else if( tok == LSTOK_RPAREN ){
            while(sp){
                tok = stack[--sp];
                if( tok == LSTOK_LPAREN ) break;
                if( pc >= ELEMENTSIN(progmem) ) return "program too big";
                progmem[pc++] = tok;
            }
            if( tok != LSTOK_LPAREN ) 	return "missing )";
        }else{
            check -= ops[tkn].argc - 1;
            while(sp && stack[sp-1] > tok && stack[sp-1] != LSTOK_LPAREN ){
                if( pc >= ELEMENTSIN(progmem) ) return "program too big";
                progmem[pc++] = stack[--sp];
            }
            if( sp >= ELEMENTSIN(stack) ) return "stack overflow";
            stack[sp++] = tok;
        }
    }

    if( tok != LSTOK_THEN ) 		return "expected 'then'";

    // pop off remaining ops
    while(sp){
        tok = stack[--sp];
        if( tok == LSTOK_LPAREN ) 	return "too many (";
        if( pc >= ELEMENTSIN(progmem) ) return "program too big";
        progmem[pc++] = tok;
    }

    // simple check. you can pass quite a lot of syntax errors through
    if( check != 1 )	return "invalid expr";		// wrong # args in expr
    *ppc = pc;
    return 0;
}

static inline int
read_line(FILE *f, int pos){

    // skip leading white
    while(1){
        int c = next_char(f);
        if( isspace(c) ) continue;
        ungetc(c, f);
        break;
    }

    if( !fgets(progmem + pos, sizeof(progmem) - pos - 32, f) )
        return 0;

    // measure + remove \n
    short i=0;
    while(1){
        if( progmem[pos + i] == '\n' || !progmem[pos + i] ){
            progmem[pos + i] = 0;
            return i;
        }
        i++;
    }
}


static int
parse_line(char *buf, char **argv){

    int argc = 0;
    char *p = buf;
    char qc;

    while( *p && (argc < ARGLEN) ){

        /* end of input */
        if( ! *p ){
            break;
        }

        /* single quoted arg */
        if( *p == '\'' ){
            p++;
            argv[argc++] = p;
            while( *p && *p!='\'' ){
                p++;
            }
            /* terminate arg */
            if( *p ) *p++ = 0;
            continue;
        }

        /* double-quoted arg or bare-word until space : $var are expanded */
        if( *p == '"' ){
            qc = 1;
            p ++;
        }else
            qc = 0;

        argv[argc++] = p;
        while( *p && (qc ? (*p!='"') : !isspace(*p)) ){
            p ++;
        }
        if( *p ) *p++ = 0;
        continue;
    }

    return argc;
}


// -1 => eof, LSTOK_ELSE, LSTOK_ENDIF
static int
compile_commands(FILE *f, short *xpc, short *cmd){
    short pc=*xpc;
    short pc0=pc;
    short cpc=pc+3;	// skip space for jmp
    *cmd = 0;

    // compile commands into:
    // jmp X, { parsed cmd data, nextcmd, argc, argv, argv..., }

    progmem[pc] = LSTOK_JMP;	// jump target will be filled in at end

    while(1){
        // read line
        short len = read_line(f, cpc);
        if( !len ) return -1;
        //printf("read %d [%s]\n", len, progmem + cpc);
        if( !strcmp(progmem + cpc, "else") || !strcmp(progmem + cpc, "end") ){
            *xpc = cpc;
            // fill in jmp target
            progmem[pc + 1] = cpc & 0xFF;
            progmem[pc + 2] = cpc >> 8;
            //printf("jmp [%x] = %x\n", pc + 1, cpc);
            if( !strcmp(progmem + cpc, "else") ) return LSTOK_ELSE;
            return LSTOK_ENDIF;
        }

        // parse line
        char argc = parse_line(progmem + cpc, (char**)(progmem + cpc + len + 4));
        // [command text\0] [next] [argc] [argv...]
        progmem[ cpc + 1 + len ] = 0;
        progmem[ cpc + 2 + len ] = 0;
        progmem[ cpc + 3 + len ] = argc;

        // link: cmdptr points to next
        *cmd = cpc + 1 + len;
        // move ahead
        cmd = (short*)(progmem + len + cpc + 1);	// next command goes here
        cpc += 3 + len + 1 + argc * sizeof(char**);
    }

    // RSN - check too big
}

static int
load_script(const char *file){
    uchar buf[8];
    FILE *f = fopen(file, "r");

    if(!f){
        fprintf(STDERR, "cannot open file\n");
        return 0;
    }

    progmem[0] = LSTOK_END;
    log_chkval = 0;

    short pc=0;
    short ifcmd, elcmd;
    schar tkn;
    uchar tok;
    const char *error=0;

    while(1){
        // read 'if' or eof, nothing else is valid
        tkn = read_next_token(f, buf, sizeof(buf), &tok);
        if( tkn == ENDOFILE ) break;
        if( tok != LSTOK_IF ){
            error = "expected 'if'";
            break;
        }

        // read expr, then
        error = compile_expr(f, buf, sizeof(buf), &pc);
        if( error ) break;

        // read cmds, else, cmds, end
        ifcmd = elcmd = 0;
        tok = compile_commands(f, &pc, &ifcmd);
        if( tok == LSTOK_ELSE ){
            tok = compile_commands(f, &pc, &elcmd);
        }
        if( tok != LSTOK_ENDIF ){
            error = "expected 'end'";
            break;
        }

        progmem[pc++] = LSTOK_IF;
        progmem[pc++] = ifcmd;
        progmem[pc++] = ifcmd >> 8;
        progmem[pc++] = elcmd;
        progmem[pc++] = elcmd >> 8;
    }

    progmem[pc++] = LSTOK_END;

    if( error ){
        progmem[pc=0] = LSTOK_END;
        fprintf(STDERR, "error: %s\n", error);
    }

    return pc;
}

void
run_commands(int pc){
    // [next] [argc] [argv...]
    short i;

    while( pc ){
        shell_eval( progmem[pc + 2], (const char**)(progmem + pc + 3), 0);
        pc = *(short*)(progmem + pc);
    }
}

#define LS1OP(op)	\
            stack[sp-1] = op stack[sp-1]

#define LS2OP(op)	\
            sp --;	\
            stack[sp-1] = stack[sp-1] op stack[sp]

void
run_bytecode(void){
    short pc = 0;
    short sp = 0;
    short a;

    while(1){
        uchar b = progmem[pc++];

        switch(b){
        case LSTOK_END:		return;
        case LSTOK_NOP:		break;
        case LSTOK_JMP:		pc = *(short*)(progmem + pc);	break;
        case LSTOK_VAR:
            a = progmem[pc++];
            stack[sp++] = log_values[a];
            break;
        case LSTOK_CONST:
            stack[sp++] = *(short*)(progmem + pc);
            pc += 2;
            break;
        case LSTOK_NOT:		LS1OP( ! );	break;
        case LSTOK_INV:		LS1OP( ~ );	break;
        case LSTOK_LAND:	LS2OP( && );	break;
        case LSTOK_LOR:		LS2OP( || );	break;
        case LSTOK_BAND:	LS2OP( & );	break;
        case LSTOK_BOR:		LS2OP( | );	break;
        case LSTOK_BXOR:	LS2OP( ^ );	break;
        case LSTOK_EQ:		LS2OP( == );	break;
        case LSTOK_NEQ:		LS2OP( != );	break;
        case LSTOK_GT:		LS2OP( > );	break;
        case LSTOK_GE:		LS2OP( >= );	break;
        case LSTOK_LT:		LS2OP( < );	break;
        case LSTOK_LE:		LS2OP( >= );	break;
        case LSTOK_PLUS:	LS2OP( + );	break;
        case LSTOK_MINUS:	LS2OP( - );	break;
        case LSTOK_MULT:	LS2OP( * );	break;
        case LSTOK_DIV:		LS2OP( / );	break;
        case LSTOK_MOD:		LS2OP( % );	break;
        case LSTOK_SHL:		LS2OP( << );	break;
        case LSTOK_SHR:		LS2OP( >> );	break;

        case LSTOK_IF:
            if( stack[--sp] ){
                run_commands( *(short*)(progmem + pc) );
            }else{
                run_commands( *(short*)(progmem + pc + 2) );
            }
            pc += 4;
            break;
        }
    }
}

#ifdef KTESTING

#define DPYOP( x )	printf(" " #x " ")
void
dump_bytecode(void){
    short pc = 0;
    short sp = 0;
    short a;

    while(1){
        uchar b = progmem[pc++];

        switch(b){
        case LSTOK_END:		printf("END\n"); return;
        case LSTOK_NOP:		printf("nop\n"); break;
        case LSTOK_JMP:
            pc = *(short*)(progmem + pc);
            printf("jmp %x\n", pc);
            break;
        case LSTOK_VAR:
            a = progmem[pc++];
            printf("var(%d) ", a);
            break;
        case LSTOK_CONST:
            printf("%d ", *(short*)(progmem + pc));
            pc += 2;
            break;
        case LSTOK_NOT:		DPYOP( ! );	break;
        case LSTOK_INV:		DPYOP( ~ );	break;
        case LSTOK_LAND:	DPYOP( && );	break;
        case LSTOK_LOR:		DPYOP( || );	break;
        case LSTOK_BAND:	DPYOP( & );	break;
        case LSTOK_BOR:		DPYOP( | );	break;
        case LSTOK_BXOR:	DPYOP( ^ );	break;
        case LSTOK_EQ:		DPYOP( == );	break;
        case LSTOK_NEQ:		DPYOP( != );	break;
        case LSTOK_GT:		DPYOP( > );	break;
        case LSTOK_GE:		DPYOP( >= );	break;
        case LSTOK_LT:		DPYOP( < );	break;
        case LSTOK_LE:		DPYOP( >= );	break;
        case LSTOK_PLUS:	DPYOP( + );	break;
        case LSTOK_MINUS:	DPYOP( - );	break;
        case LSTOK_MULT:	DPYOP( * );	break;
        case LSTOK_DIV:		DPYOP( / );	break;
        case LSTOK_MOD:		DPYOP( % );	break;
        case LSTOK_SHL:		DPYOP( << );	break;
        case LSTOK_SHR:		DPYOP( >> );	break;

        case LSTOK_IF:
            printf("if then %x else %x\n", *(short*)(progmem + pc), *(short*)(progmem + pc + 2));
            pc += 4;
            break;
        }
    }
}
#endif

void
compile_script(void){

    if( ! logger_script[0] ){
        progmem[0] = 0;
        return;
    }
    proglen = load_script( logger_script );
}

#ifdef KTESTING
DEFUN(testscript, "test script")
{
    if( argc > 1 )
        proglen = load_script( argv[1] );
    else
        proglen = load_script( logger_script );

    printf("script %d bytes used of %d\n", proglen, sizeof(progmem));
    hexdump(progmem, proglen);
    dump_bytecode();
    run_bytecode();
    return 0;
}
#endif

#ifdef STANDALONE

int shell_eval(int c, const char**v){}

int find_pin(int x){ return 0xAA; }

void
hexdump(const unsigned char *d, int len){
    int i;
    int col = 0;
    char txt[17];
    txt[16] = 0;

    printf("\n");

    for(i=0; i<len; i++){
        if( !col )       printf("%08.8X:", d + i);
        if(! (col % 4 )) printf(" ");

        printf(" %x%x", (d[i]&0xF0) >> 4, (d[i]&0x0F));
        txt[col] = (d[i] >= 0x20 && d[i] <= 0x7e) ? d[i] : '.';

        if( ++col == 16){
            printf("  %s\n", txt);
            col = 0;
        }
    }

    // ...
    printf("\n");
}

int
main(int argc, char**argv){

    if( argc < 2 ) return;
    memset(progmem, 0xFF, sizeof(progmem));
    proglen = load_script( argv[1] );
    hexdump(progmem, proglen);
    dump_bytecode();
    run_bytecode();

}

#endif

/*
  config script:

  # comment

  if A0 > 123 & B4 | A7 < 12 then
      # 1 => log this data, 0 => don't log
      # number - log this many samples
      logger_count = 1
  end

  if A2 & A3 > 100 then
      setpin A6 127
  end

  if A5 then
      play abcabc
  end

*/

