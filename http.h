//
// Created by 田韵豪 on 2022/10/28.
//

#ifndef TYHTTPD_HTTP_H
#define TYHTTPD_HTTP_H

#include "stream.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_SHORT_LEN 30
#define MAX_LEN 200

struct HttpHeader {
  char name[MAX_SHORT_LEN];
  char value[MAX_LEN];
  struct HttpHeader* next;
};

struct HttpRequest {
  char method[10];
  char uri[MAX_LEN];
  struct HttpHeader* headers;
};

struct HttpRequest* ParseRequest(struct MyStream* stream);
void FreeRequest(struct HttpRequest* req);
int UrlDecode(char *buf, int blen, const char *src, int slen);

struct RequestRange {
  off_t start;
  off_t end;
};

bool ParseRangeHeader(const char* content, struct RequestRange* ptr);


#endif //TYHTTPD_HTTP_H
