//
// Created by 田韵豪 on 2022/10/28.
//

#ifndef TYHTTPD_STREAM_H
#define TYHTTPD_STREAM_H

#include <string.h>

struct MyStream;

typedef ssize_t (*read_function)(struct MyStream* stream, void* buf, size_t num);
typedef ssize_t (*write_function)(struct MyStream* stream, const void* buf, size_t num);
typedef void (*destroy_function)(struct MyStream* stream);

struct MyStream {
  void* priv;
  void* userdata;
  read_function read;
  write_function write;
  // Stream is automatically freed when calling destroy
  destroy_function destroy;
};

struct MyStream* InitTcpStream(int fd);

inline static ssize_t WriteString(struct MyStream* stream, const char* str) {
  return stream->write(stream, str, strlen(str));
}

#endif //TYHTTPD_STREAM_H
