#include "tlstream.h"

void load_tls_cert(SSL_CTX *ctx, const char *cert_file, const char *key_file)
{
  if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0)
  {
    ERR_print_errors_fp(stderr);
    exit(-1);
  }
  if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0)
  {
    ERR_print_errors_fp(stderr);
    exit(-1);
  }
  if (!SSL_CTX_check_private_key(ctx))
  {
    fprintf(stderr, "Private key does not match the certificate public key");
    exit(-1);
  }
}

SSL_CTX *InitTlsContext(const char *cert_file, const char *key_file)
{
  SSL_load_error_strings();
  SSL_library_init();
  SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
  if (ctx == NULL)
  {
    ERR_print_errors_fp(stderr);
    return NULL;
  }
  SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

  load_tls_cert(ctx, cert_file, key_file);
  return ctx;
}

void DestroyTlsContext(SSL_CTX *ctx)
{
  SSL_CTX_free(ctx);
}

SSL *tls_accept(struct MyStream *stream)
{
  if (stream->userdata != NULL)
  {
    return stream->userdata;
  }
  ssl_stream_priv *priv = stream->priv;
  SSL_CTX *ctx = priv->ssl;
  int fd = priv->fd;
  SSL *ssl = SSL_new(ctx);
  SSL_set_fd(ssl, fd);

  int ret = SSL_accept(ssl);
  if (ret <= 0)
  {
    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
    {
      return stream->userdata=ctx;
    }
    else
    {
      ERR_print_errors_fp(stderr);
      return stream->userdata=ctx;
    }
  }
  return stream->userdata = ssl;
}

static void tls_close(struct MyStream *stream)
{
  SSL *ssl = tls_accept(stream);
  ssl_stream_priv *priv = stream->priv;
  if (ssl != NULL && ssl != priv->ssl)
  {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }
  close(priv->fd);
  SSL_CTX_free(priv->ssl);
  free(priv);
  free(stream);
}

static ssize_t tls_read(struct MyStream *stream, void *buffer, size_t size)
{
  SSL *ssl = tls_accept(stream);
  if (ssl == NULL)
  {
    tls_close(stream);
    return -1;
  }
  return SSL_read(ssl, buffer, size);
}

static ssize_t tls_write(struct MyStream *stream, const void *buffer, size_t size)
{
  SSL *ssl = tls_accept(stream);
  if (ssl == NULL)
  {
    tls_close(stream);
    return -1;
  }
  return SSL_write(ssl, buffer, size);
}

struct MyStream *InitTlsStream(int fd)
{
  struct MyStream *stream = malloc(sizeof(struct MyStream));
  stream->priv = malloc(sizeof(ssl_stream_priv));
  ssl_stream_priv *ssl_priv = stream->priv;
  ssl_priv->fd = fd;
  ssl_priv->ssl = InitTlsContext(CERT_FILE, KEY_FILE);
  stream->userdata = NULL;
  stream->read = tls_read;
  stream->write = tls_write;
  stream->destroy = tls_close;
  return stream;
}
