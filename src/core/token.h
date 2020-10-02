#include <stdint.h>

#ifndef __TOKEN_H
#define __TOKEN_H

#define ERROR__UNEXPECTED -1
#define ERROR__STACK      -2  // stack didn't balance
#define ERROR__INTERNAL   -3  // internal error
#define ERROR__TODO       -4

// empty: will not contain text
#define TOKEN_EOF       0

// special
#define TOKEN_LIT       1   // named thing

// fixed: will always be the same, or in the same set
#define TOKEN_SEMICOLON 2   // ;
#define TOKEN_OP        3   // includes 'in', 'instanceof', 'of', 'void'
#define TOKEN_COLON     4   // used in label or dict
#define TOKEN_BRACE     5   // {
#define TOKEN_ARRAY     6   // [
#define TOKEN_PAREN     7   // (
#define TOKEN_TERNARY   8   // starts ternary block, "? ... :"
#define TOKEN_CLOSE     9   // '}', ']', ')', or ternary ':'

// variable: could be anything
#define TOKEN_STRING    10
#define TOKEN_REGEXP    11  // literal "/foo/g", not "new RegExp('foo')"
#define TOKEN_NUMBER    12
#define TOKEN_SYMBOL    13  // this and above must be literal types
#define TOKEN_KEYWORD   14
#define TOKEN_LABEL     15  // to the left of a ':', e.g. 'foo:'


#define _TOKEN_MAX      15


#define SPECIAL__SAMELINE 1

struct token {
  char *vp;  // void-pointer (before token)
  char *p;
  int len;
  int line_no;
  int type;
  uint32_t special;
};


int blep_token_init(char *, int);
int blep_token_update(int);
int blep_token_next();
int blep_token_peek();

int blep_token_set_restore();
int blep_token_restore();

inline int blep_token_is_symbol_part(char);


#define STACK_SIZE    256



typedef struct {
  struct token curr;  // cursor before head
  struct token peek;  // also before head if p is !NULL

  int line_no;  // line_no at head
  char *at;     // head pointer
  char *end;    // end of input (must point to NULL)

  // depth/stack at head (just used for balancing)
  int depth;
  int stack[STACK_SIZE];

  struct token restore__curr;
  int restore__line_no;
  char *restore__at;
  int restore__depth;
} tokendef;

// global
#ifdef EMSCRIPTEN
#define td ((tokendef *) 20)
#else
extern tokendef _td;
#define td (&_td)
#endif

#endif//__TOKEN_H