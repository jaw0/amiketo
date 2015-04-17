/*
  Copyright (c) 2015
  Author: Jeff Weisberg <jaw @ tcp4me.com>
  Created: 2015-Apr-12 15:51 (EDT)
  Function: 

*/

#ifndef __script_h__
#define __script_h__

enum {
    LSTOK_END		= 0,
    LSTOK_NOP,
    LSTOK_JMP,
    LSTOK_VAR,
    LSTOK_CONST,
    LSTOK_IF,

    // in precedence order
    LSTOK_NOT,
    LSTOK_INV,
    LSTOK_MULT,
    LSTOK_DIV,
    LSTOK_MOD,
    LSTOK_PLUS,
    LSTOK_MINUS,
    LSTOK_SHL,
    LSTOK_SHR,
    LSTOK_BAND,		// ruby order
    LSTOK_BOR,
    LSTOK_BXOR,
    LSTOK_GT,
    LSTOK_GE,
    LSTOK_LT,
    LSTOK_LE,
    LSTOK_EQ,
    LSTOK_NEQ,
    LSTOK_LAND,
    LSTOK_LOR,

    // syntactic, not emitted in bytecode
    LSTOK_LPAREN,
    LSTOK_RPAREN,
    LSTOK_THEN,
    LSTOK_ELSE,
    LSTOK_ENDIF,
    LSTOK_CMD,
};


#endif /* __script_h__ */
