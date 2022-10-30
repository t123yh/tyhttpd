#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>

#include "stream.h"
#ifndef _TLSTREAM_H_
#define _TLSTREAM_H_

struct MyStream *InitTlsStream(int fd);

#ifndef CERT_FILE
#define CERT_FILE "keys/cnlab.cert"
#endif

#ifndef KEY_FILE
#define KEY_FILE "keys/cnlab.prikey"
#endif

typedef struct ssl_stream_priv
{
    SSL_CTX *ssl;
    int fd;
} ssl_stream_priv;

#endif
