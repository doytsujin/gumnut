#include <string.h>
#include <inttypes.h>
#include "token.h"
#include "utils.h"
#include "parser.h"

int prsr_token(char *buf, int (*fp)(token *)) {
  tokendef d;
  bzero(&d, sizeof(d));
  d.buf = buf;
  d.len = strlen(buf);
  d.depth = 1;
  d.line_no = 1;

  token out;
  int ret;
  while (!(ret = prsr_next_token(&d, &out))) {
    fp(&out);
  }
  return ret;
}
