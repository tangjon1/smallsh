#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

// The following code is modified from course material for use in the SmallSh program

char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub, int first_instance_only)
{
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle),
         sub_len = strlen(sub);
  for (int i = 0; (str = strstr(str, needle)); i++) {
    if ((i == 1) && (first_instance_only != 0)) {
      break;
    }
    ptrdiff_t off = str - *haystack;
    if (sub_len > needle_len) {
      str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
      if (!str) goto exit;
      *haystack = str;
      str = *haystack + off;
    }

    memmove(str + sub_len, str + needle_len, haystack_len + 1 - off - needle_len);
    memcpy(str, sub, sub_len);
    haystack_len = haystack_len + sub_len - needle_len;
    str += sub_len;
  }
  str = *haystack;
  if (sub_len < needle_len) {
    str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
    if (!str) goto exit;
    *haystack = str;
  }
exit:
  return str;
}
