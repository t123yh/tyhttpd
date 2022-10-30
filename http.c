//
// Created by 田韵豪 on 2022/10/28.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "http.h"

enum HttpParserStatus {
  kMethod,
  kUri,
  kVersion,
  kHeaderName,
  kHeaderValue,
  kEnding,
};

struct HttpRequest* ParseRequest(struct MyStream* stream) {
  struct HttpRequest* req = malloc(sizeof(struct HttpRequest));
  memset(req, 0, sizeof(struct HttpRequest));

  const size_t kBufferSize = 1000;
  uint8_t* buffer = malloc(kBufferSize);

  struct HttpHeader** current_header = &req->headers;
  int position = 0;

  uint8_t last = 0;
  enum HttpParserStatus status = kMethod;
  for(;;) {
    ssize_t amount = stream->read(stream, buffer, kBufferSize);

    for (int i = 0; i < amount; i++) {
      uint8_t current = buffer[i];
      switch(status) {
        case kMethod:
          if (current != ' ') {
            if (position < sizeof(req->method) - 1) {
              req->method[position] = current;
              position++;
            } else {
              fprintf(stderr, "Invalid request: Method too loong");
              goto err_exit;
            }
          } else {
            req->method[position] = 0;
            position = 0;
            status = kUri;
          }
          break;
        case kUri:
          if (current != ' ') {
            if (position < sizeof(req->uri) - 1) {
              req->uri[position] = current;
              position++;
            } else {
              fprintf(stderr, "Invalid request: URI too loong");
              goto err_exit;
            }
          } else {
            req->uri[position] = 0;
            position = 0;
            status = kVersion;
          }
          break;
        case kVersion:
          if (current == '\r') {
            // Do nothing
          }
          else if (current == '\n') {
            if (last == '\r') {
              position = 0;
              status = kHeaderName;
            } else {
              fprintf(stderr, "Invalid request: CR not followed by LF while parsing version");
              goto err_exit;
            }
          }
          break;
        case kHeaderName:
          if (current == '\r') {
            // End of request
            status = kEnding;
          } else if (current != ':') {
            if (*current_header == NULL) {
              *current_header = malloc(sizeof(struct HttpHeader));
              (*current_header)->next = NULL;
            }
            if (position < MAX_SHORT_LEN - 1) {
              (*current_header)->name[position] = current;
              position++;
            } else {
              fprintf(stderr, "Invalid request: Header name too loong");
              goto err_exit;
            }
          } else {
            if (*current_header == NULL) {
              fprintf(stderr, "Invalid request: Header name is empty");
              goto err_exit;
            }
            (*current_header)->name[position] = 0;
            position = 0;
            status = kHeaderValue;
          }
          break;
        case kHeaderValue:
          if (current != '\r' && current != '\n') {
            // Skip whitespace
            if (!(position == 0 && current == ' ')) {
              if (position < MAX_LEN - 1) {
                (*current_header)->value[position] = current;
                position++;
              }
            }
          } else if (current == '\r') {
            (*current_header)->value[position] = 0;
            position = 0;
          } else if (current == '\n') {
            if (last == '\r') {
              current_header = &(*current_header)->next;
              position = 0;
              status = kHeaderName;
            } else {
              fprintf(stderr, "Invalid request: CR not followed by LF while parsing header");
              goto err_exit;
            }
          }
          break;
        case kEnding:
          if (current == '\n' && last == '\r') {
            // Really ended
            // TODO: Part of the request body (if present) is in buffer now, and is
            // TODO: actually dropped here by freeing buffer. Since we are only dealing
            // TODO: with GET requests, no request body handling is required.
            free(buffer);
            return req;
          } else {
            fprintf(stderr, "Invalid request: CR not followed by LF while parsing ending");
            goto err_exit;
          }
          break;
      }

      last = current;
    }
  }

  err_exit:
  free(buffer);
  FreeRequest(req);
  return NULL;
}

void FreeRequest(struct HttpRequest* req) {
  if (req != NULL) {
    struct HttpHeader *cur = req->headers;
    while (cur != NULL) {
      struct HttpHeader *f = cur;
      cur = cur->next;
      free(f);
    }
    free(req);
  }
}

int UrlDecode(char *buf, int blen, const char *src, int slen)
{
  int i;
  int len = 0;

#define hex(x) \
	(((x) <= '9') ? ((x) - '0') : \
		(((x) <= 'F') ? ((x) - 'A' + 10) : \
			((x) - 'a' + 10)))

  for (i = 0; (i < slen) && (len < blen); i++)
  {
    if (src[i] != '%') {
      buf[len++] = src[i];
      continue;
    }

    if (i + 2 >= slen || !isxdigit(src[i + 1]) || !isxdigit(src[i + 2]))
      return -2;

    buf[len++] = (char)(16 * hex(src[i+1]) + hex(src[i+2]));
    i += 2;
  }
  buf[len] = 0;

  return (i == slen) ? len : -1;
}

bool ParseRangeHeader(const char* content, struct RequestRange* ptr) {
  int ret = sscanf(content, "bytes=%ld-%ld", &ptr->start, &ptr->end);
  if (ret == 1) {
    ptr->end = 0x3FFFFFFFFFFFFFFFull;
    ret++;
  }
  return ret == 2;
}
