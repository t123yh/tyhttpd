//
// Created by 田韵豪 on 2022/10/28.
//

#include <unistd.h>
#include <stdlib.h>
#include "stream.h"

struct tcp_stream_priv {
  int fd;
};

static ssize_t tcp_read_function(struct MyStream* stream, void* buf, size_t num) {
  struct tcp_stream_priv* tcp_priv = stream->priv;
  return read(tcp_priv->fd, buf, num);
}

static ssize_t tcp_write_function(struct MyStream* stream, const void* buf, size_t num) {
  struct tcp_stream_priv* tcp_priv = stream->priv;
  return write(tcp_priv->fd, buf, num);
}

static void tcp_close_function(struct MyStream* stream) {
  struct tcp_stream_priv* tcp_priv = stream->priv;
  close(tcp_priv->fd);
  free(tcp_priv);
  free(stream);
}

struct MyStream* InitTcpStream(int fd) {
  struct MyStream* stream = malloc(sizeof(struct MyStream));
  stream->priv = malloc(sizeof(struct tcp_stream_priv));
  struct tcp_stream_priv* tcp_priv = stream->priv;
  tcp_priv->fd = fd;
  stream->userdata = NULL;
  stream->read = tcp_read_function;
  stream->write = tcp_write_function;
  stream->destroy = tcp_close_function;
  return stream;
}
