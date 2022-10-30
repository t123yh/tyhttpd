#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>

#include "stream.h"
#ifndef _TLSTREAM_H_
#define _TLSTREAM_H_

struct MyStream *InitTlsStream(int fd);

#ifndef CERT_FILE
#define CERT_FILE "server.crt"
#endif

#ifndef KEY_FILE
#define KEY_FILE "server.key"
#endif

typedef struct ssl_stream_priv
{
    SSL_CTX *ssl;
    int fd;
} ssl_stream_priv;

#endif
