/*
 * Copyright 2020 Sam Thorogood.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#include <ctype.h>

int strline(char *p, char *end) {
  const char *start = p;

  for (;;) {
    if (p[0] == '\n') {
      break;
    } else if (!p[0]) {
      if (p == end) {
        break;
      }
    }
    ++p;
  }

  return p - start;
}

// Convenience wrapper for the generated helper code.
#include "tokens/helper.c"
