//
// Remote Radio Control Protocol (RRCP)
//
// Define a grammar called rrcp
grammar rrcp;

//
// rules:
//

//TODO(CK): rrcp: (LF MU CR)+ EOF ;
rrcp : (Newline* MU Newline?)+ EOF ;

//TODO(CK): MU: TU (SP TU)* ;
// NOTE: with optional LineComment for test
MU
    : MIB_PATH SP Optional? TU (SP TU)* LineComment?
    | Optional? RESP_TU (SP RESP_TU)* LineComment?
    | MuErrorStatus SP? Optional? LineComment?
    | SP? LineComment
    ;

MIB_PATH:
    'M:' ALPHANUM ('.' ALPHANUM)*
    ;

// NOTE: the LogicalAddress must not used with RESP_TU?
fragment
Optional:
    (LogicalAddress SP)? (MessageID SP)?
    ;

LogicalAddress:
    'L:' NUM
    ;

MessageID:
    NUM
    ;

TU
    :  REQ SP* CU                       // with optional space before CU!
    ;

RESP_TU
    : RESP SP* TuErrorStatus CMD
    | RESP SP* CU
    | ACK                              // NOTE: trap or set response without CU!
    ;

REQ: 'G' | 'S' | 'T' ;

RESP: 'g' | 's' | 'd' ;

ACK: [ts] ;

MuErrorStatus:
    'E:' NUM
    ;

TuErrorStatus:
    NUM
    ;

CU
    : CMD PU? (';' CMD PU?)*
    ;

CMD:
    ALPHA
    ;

fragment
PU:
    SP* PARAMETER (',' PARAMETER)*     // with optional space after command!
    ;

PARAMETER
    : STRING
    | INT
    | FLOAT
    | BINARY
    ;

BINARY
    : '#' NUM ':' Base64Digit+
    ;

fragment
Base64Digit
    : [0-9a-zA-Z+/=]
    ;

//XXX STRING : '"'~('"')*'"' ;        // NOTE: without EscapeSequence
STRING : StringLiteral ;

fragment
EscapeSequence
    :   SimpleEscapeSequence
    ;

fragment
SimpleEscapeSequence
    :   '\\' ['"?abfnrtv\\]
    ;

StringLiteral
    :   '"' SCharSequence? '"'
    ;

fragment
SCharSequence
    :   SChar+
    ;

fragment
SChar
    :   ~["\\\n]
    |   EscapeSequence
    |   '\\\n' // Added line
    ;

Newline
    :   [\n]
        -> skip
    ;

LineComment
    :   '//' ~[\n]*
        -> skip
    ;

//======================================================
fragment
NUM :        [0-9]+ ;           // match unsigned decimal numbers
fragment
INT :   [+-]?[0-9]+ ;           // signed decimal numbers
fragment
FLOAT : [+-]?[0-9]+'.'[0-9]+ ;  // rational numbers
fragment
ALPHA :     [a-zA-Z]+ ;         // match alpha identifiers
fragment
ALPHANUM :  [0-9a-zA-Z]+ ;      // match alphaNum words
//XXX NO! WS : [ \t\r\n]+ -> skip ; // skip spaces, tabs, newlines
SP : ' ' -> skip ;
//TODO(CK): CR : '\n' ;  # 0x0d
//TODO(CK): LF : '\r' ;  # 0x0a
//======================================================

//
// vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4 syntax=antlr
//
