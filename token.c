#include <ctype.h>
#include <string.h>
#include "token.h"
#include "utils.h"

#define FLAG__PENDING_T_BRACE 1
#define FLAG__RESUME_LIT      2

#define _CONSUME(_len, _type) (eat->len = _len, eat->type = _type, 0);

typedef struct {
  int len;
  int type;
} eat_out;

static char peek_char(tokendef *d, int len) {
  int out = d->curr + len;
  if (out < d->len) {
    return d->buf[out];
  }
  return 0;
}

static int stack_inc(tokendef *d, int t_brace) {
  if (d->depth == __STACK_SIZE) {
    return ERROR__STACK;
  }

  uint32_t *p = d->stack + (d->depth >> 5);
  uint32_t set = (uint32_t) 1 << (d->depth & 31);
  if (t_brace) {
    *p |= set;  // set bit
  } else {
    *p &= ~set; // clear bit
  }

  ++d->depth;
  return 0;
}

static int stack_dec(tokendef *d) {
  if (!d->depth) {
    return ERROR__STACK;
  }
  --d->depth;

  uint32_t *p = d->stack + (d->depth >> 5);
  uint32_t check = (uint32_t) 1 << (d->depth & 31);

  if (*p & check) {
    return 1;
  }
  return 0;
}

static int consume_slash_op(char *p) {
  // can match "/" or "/="
  if (p[1] == '=') {
    return 2;
  }
  return 1;
}

static int consume_slash_regexp(char *p) {
  char *start = p;
  int is_charexpr = 0;

  for (;;) {
    ++p;

    switch (*p) {
      case '/':
        // nb. already known not to be a comment `//`
        if (is_charexpr) {
          break;
        }

        // eat trailing flags
        do {
          ++p;
        } while (isalnum(*p));

        // fall-through
      case 0:
      case '\n':
        return (p - start);

      case '[':
        is_charexpr = 1;
        break;

      case ']':
        is_charexpr = 0;
        break;

      case '\\':
        ++p;  // ignore next char
    }
  }
}

// nb. changes ->flag, ->depth and ->line_no
static int eat_token(tokendef *d, eat_out *eat) {
  int flag = d->flag;
  d->flag = 0;

  // look for EOF
  char c = peek_char(d, 0);
  if (!c) {
    _CONSUME(0, TOKEN_EOF);
    if (d->depth) {
      return ERROR__STACK;
    }
    return 0;
  }

  // flag states
  if (flag & FLAG__PENDING_T_BRACE) {
    _CONSUME(2, TOKEN_T_BRACE);
    return stack_inc(d, 1);
  } else if (flag & FLAG__RESUME_LIT) {
    goto resume_lit;
  }

  // unambiguous simple ascii characters
  switch (c) {
    case ';':
      return _CONSUME(1, TOKEN_SEMICOLON);

    case '?':
      return _CONSUME(1, TOKEN_TERNARY);

    case ':':
      return _CONSUME(1, TOKEN_COLON);

    case ',':
      return _CONSUME(1, TOKEN_COMMA);

    case '(':
      _CONSUME(1, TOKEN_PAREN);
      return stack_inc(d, 0);

    case '[':
      _CONSUME(1, TOKEN_ARRAY);
      return stack_inc(d, 0);

    case '{':
      _CONSUME(1, TOKEN_BRACE);
      return stack_inc(d, 0);

    case ')':
      _CONSUME(1, TOKEN_CLOSE);
      return stack_dec(d);

    case ']':
      _CONSUME(1, TOKEN_CLOSE);
      return stack_dec(d);

    case '}': {
      int ret = stack_dec(d);
      if (ret < 0) {
        return ret;
      } else if (ret > 0) {
        d->flag = FLAG__RESUME_LIT;
      }
      return _CONSUME(1, TOKEN_CLOSE);
    }
  }

  // ops: i.e., anything made up of =<& etc
  // note: 'in' and 'instanceof' are ops in most cases, but here they are lit
  do {
    const char start = c;
    int len = 0;
    int allowed;  // how many ops of the same type we can safely consume

    if (strchr("=&|^~!%+-", c)) {
      allowed = 1;
    } else if (c == '*' || c == '<') {
      allowed = 2;  // exponention operator **, or shift
    } else if (c == '>') {
      allowed = 3;  // right shift, or zero-fill right shift
    } else {
      break;
    }

    while (len < allowed) {
      c = peek_char(d, ++len);
      if (c != start) {
        break;
      }
    }

    if (start == '=' && c == '>') {
      // arrow function: nb. if this is after a newline, it's invalid (doesn't generate ASI), ignore
      return _CONSUME(2, TOKEN_ARROW);
    } else if (c == start && strchr("+-|&", start)) {
      ++len;  // eat --, ++, || or &&: but no more
    } else if (c == '=') {
      // consume a suffix '=' (or whole ===, !==)
      c = peek_char(d, ++len);
      if (c == '=' && (start == '=' || start == '!')) {
        ++len;
      }
    }

    return _CONSUME(len, TOKEN_OP);
  } while (0);

  // strings
  if (c == '\'' || c == '"' || c == '`') {
    char start = c;
    int len = 0;
    goto start_string;
resume_lit:
    start = '`';
    len = -1;
start_string:
    while ((c = peek_char(d, ++len))) {
      // TODO: strchr for final, and check
      if (c == start) {
        ++len;
        break;
      } else if (c == '\\') {
        c = peek_char(d, ++len);
      } else if (start == '`' && c == '$' && peek_char(d, len + 1) == '{') {
        d->flag = FLAG__PENDING_T_BRACE;  // next is "${"
        break;
      }
      if (c == '\n') {
        // TODO: not allowed in quoted strings
        ++d->line_no;  // look for \n
      }
    }
    return _CONSUME(len, TOKEN_STRING);
  }

  // number: "0", ".01", "0x100"
  char next = peek_char(d, 1);
  if (isnum(c) || (c == '.' && isnum(next))) {
    int len = 1;
    c = next;
    for (;;) {
      if (!(isalnum(c) || c == '.')) {  // letters, dots, etc- misuse is invalid, so eat anyway
        break;
      }
      c = peek_char(d, ++len);
    }
    return _CONSUME(len, TOKEN_NUMBER);
  }

  // dot notation
  if (c == '.') {
    if (next == '.' && peek_char(d, 2) == '.') {
      return _CONSUME(3, TOKEN_SPREAD);  // '...' operator
    }
    return _CONSUME(1, TOKEN_DOT);  // it's valid to say e.g., "foo . bar", so separate token
  }

  // literals
  int len = 0;
  do {
    if (c == '\\') {
      ++len;  // don't care, eat whatever aferwards
      c = peek_char(d, ++len);
      if (c != '{') {
        continue;
      }
      while (c && c != '}') {
        c = peek_char(d, ++len);
      }
      ++len;
      continue;
    }

    // nb. `c < 0` == `((unsigned char) c) > 127`
    int valid = (len ? isalnum(c) : isalpha(c)) || c == '$' || c == '_' || c < 0;
    if (!valid) {
      break;
    }
    c = peek_char(d, ++len);
  } while (c);

  if (len) {
    // we don't care what this is, give to caller
    return _CONSUME(len, TOKEN_LIT);
  }

  // found nothing :(
  return _CONSUME(0, -1);
}

static int consume_comment(char *p, int *line_no) {
  if (*p != '/') {
    return 0;
  }

  char *find;
  switch (p[1]) {
    case '/':
      find = "\n";
      break;

    case '*':
      find = "*/";
      break;

    default:
      return 0;
  }

  const char *search = p + 2;
  char *end = strstr(search, find);
  if (!end) {
    // FIXME: unclosed multiline comments don't update line_no
    return strlen(p);  // trailing comment goes to EOF
  }

  int len = end - p;
  if (p[1] == '/') {
    return len;
  }

  // count \n's for multiline comment
  char *newline = (char *) search;
  for (;;) {
    newline = memchr(newline, '\n', end - newline);
    if (!newline) {
      break;
    }
    ++(*line_no);
    ++newline;
  }
  return len + 2;
}

static char *consume_space(char *p, int *line_no) {
  for (;;) {
    char c = *p;
    if (!isspace(c)) {
      return p;
    } else if (c == '\n') {
      ++(*line_no);
    }
    ++p;
  }
}

static void eat_next(tokendef *d) {
  // consume from next, repeat(space, comment [first into pending]) and next token
  char *from = d->next.p + d->next.len;

  // always consume space chars
  char *p = consume_space(from, &d->line_no);
  d->pending.p = p;
  d->pending.line_no = d->line_no;

  // match comments (C99 and long), record first in pending
  int len = consume_comment(p, &d->line_no);
  d->pending.len = len;
  d->line_after_pending = d->line_no;
  while (len) {
    p += len;
    p = consume_space(p, &d->line_no);
    len = consume_comment(p, &d->line_no);
  }

  // match real token
  d->next.line_no = d->line_no;
  d->next.p = p;

  if (d->flag & FLAG__PENDING_T_BRACE) {

  } else if (d->flag & FLAG__RESUME_LIT) {

  }

  // match real token
  if (*p == '/') {
    d->next.type = TOKEN_SLASH;
    d->next.len = 0;
    return;
  }
}

int prsr_next_token(tokendef *d, token *out, int has_value) {
  if (d->pending.len) {
    // copy pending comment out, try to yield more
    memcpy(out, &d->pending, sizeof(token));

    char *p = consume_space(d->pending.p + d->pending.len, &d->line_after_pending);
    if (p == d->next.p) {
      d->pending.len = 0;
      return 0;  // nothing to do, reached real token
    }

    // queue up upcoming comment
    d->pending.p = p;
    d->pending.line_no = d->line_after_pending;
    d->pending.len = consume_comment(p, &d->line_after_pending);

    if (!d->pending.len) {
      return ERROR__INTERNAL;
    }
    return 0;
  }

  memcpy(out, &d->next, sizeof(token));
  if (out->type == TOKEN_SLASH) {
    // consume this token as lookup can't know what it was
    if (has_value) {
      out->len = consume_slash_op(out->p);
      out->type = TOKEN_OP;
    } else {
      out->len = consume_slash_regexp(out->p);
      out->type = TOKEN_REGEXP;
    }
    d->next.len = out->len;
  }

  eat_next(d);
  return 0;

  // out->line_no = d->line_no;  // set first, in case it changes

  // eat_out eo;
  // int ret = eat_token(d, &eo, has_value);
  // if (ret) {
  //   return ret;
  // } else if (eo.type < 0) {
  //   return ERROR__TOKEN;
  // }

  // out->p = d->buf + d->curr;
  // out->len = eo.len;
  // out->type = eo.type;
  // d->curr += eo.len;

  // // point to next token
  // consume_space_lookahead(d);
  // return 0;
}

tokendef prsr_init_token(char *p) {
  tokendef d;
  bzero(&d, sizeof(d));
  d.buf = p;
  d.len = strlen(p);
  d.line_no = 1;

  d.pending.type = TOKEN_COMMENT;
  d.next.p = p;  // place next cursor

  // prsr state always points to next token
  eat_next(&d);
  return d;
}
