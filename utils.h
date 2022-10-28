//
// Created by 田韵豪 on 2022/10/28.
//

#ifndef MYHTTPD_UTILS_H
#define MYHTTPD_UTILS_H

#include <stdbool.h>

void CanonicalPath(const char *path, char *path_resolved);
bool StringEndsWith(const char * str, const char * suffix);

#endif //MYHTTPD_UTILS_H
