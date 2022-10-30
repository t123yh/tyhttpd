//
// Created by 田韵豪 on 2022/10/28.
//

#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include "utils.h"

void CanonicalPath(const char *path, char *path_resolved) {
  const char *path_cpy = path;
  char *path_res = path_resolved;

  while ((*path_cpy != '\0') && (path_cpy < (path + PATH_MAX - 2))) {
    if (*path_cpy != '/')
      goto next;

    if (path_cpy[1] == '/') {
      path_cpy++;
      continue;
    }

    if (path_cpy[1] == '.') {
      if ((path_cpy[2] == '/') || (path_cpy[2] == '\0')) {
        path_cpy += 2;
        continue;
      }

      if ((path_cpy[2] == '.') &&
          ((path_cpy[3] == '/') || (path_cpy[3] == '\0'))) {
        while ((path_res > path_resolved) && (*--path_res != '/'));

        path_cpy += 3;
        continue;
      }
    }

    next:
    *path_res++ = *path_cpy++;
  }

  if ((path_res > (path_resolved + 1)) && (path_res[-1] == '/'))
    path_res--;
  else if (path_res == path_resolved)
    *path_res++ = '/';

  *path_res = '\0';

}

bool StringEndsWith(const char * str, const char * suffix)
{
  int str_len = strlen(str);
  int suffix_len = strlen(suffix);

  return
      (str_len >= suffix_len) &&
      (0 == strcmp(str + (str_len-suffix_len), suffix));
}
