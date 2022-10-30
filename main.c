#include <stdio.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <signal.h>
#include "stream.h"
#include "tlstream.h"
#include "http.h"
#include "utils.h"

void Send404(struct MyStream *stream)
{
  const char *head = "HTTP/1.1 404 Not Found\r\n";
  const char *content = "The page you've requested could not be found.";
  char len_buf[100];
  snprintf(len_buf, sizeof(len_buf), "Content-Length: %lu\r\n", strlen(content));
  const char *type = "Content-Type: text/plain\r\n";
  const char *nl = "\r\n";
  WriteString(stream, head);
  WriteString(stream, len_buf);
  WriteString(stream, type);
  WriteString(stream, nl);
  WriteString(stream, content);
}

void Send400(struct MyStream *stream)
{
  const char *head = "HTTP/1.1 400 Bad Request\r\n";
  const char *nl = "\r\n";
  WriteString(stream, head);
  WriteString(stream, nl);
}

void Send301(struct MyStream *stream, const char *url)
{
  const char *head = "HTTP/1.1 301 Moved Permanently\r\n";
  char location_buf[100];
  snprintf(location_buf, sizeof(location_buf), "Location: %s\r\n", url);
  const char *nl = "\r\n";
  WriteString(stream, head);
  WriteString(stream, location_buf);
  WriteString(stream, nl);
}

void ServeFile(struct MyStream *stream, const char *uri, struct RequestRange *range)
{
  if (strcmp(uri, "/") == 0)
  {
    uri = "/index.html";
  }
  char path[PATH_MAX];
  getcwd(path, sizeof(path));
  int cwd_len = strlen(path);
  if (UrlDecode(path + cwd_len, sizeof(path) - cwd_len - 1, uri, strlen(uri)) < 0)
  {
    fprintf(stderr, "Invalid URL encode\n");
    Send400(stream);
    return;
  }
  char canon_path[PATH_MAX];
  CanonicalPath(path, canon_path);
  if (strncmp(canon_path, path, cwd_len) != 0)
  {
    fprintf(stderr, "Trying to read file outside of current directory\n");
    Send400(stream);
    return;
  }

  struct stat st;
  if (stat(canon_path, &st) == -1)
  {
    fprintf(stderr, "Cannot open %s: %s\n", canon_path, strerror(errno));
    Send404(stream);
    return;
  }
  if (!(st.st_mode & S_IFREG))
  {
    fprintf(stderr, "%s is not a regular file\n", canon_path);
    Send404(stream);
    return;
  }

  off_t start = range ? range->start : 0;
  off_t end = range ? (range->end + 1) : st.st_size;
  if (end > st.st_size)
  {
    end = st.st_size;
  }
  if (start >= end)
  {
    start = 0;
  }

  const char *head = range ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
  const char *acpt_rng = "Accept-Ranges: bytes\r\n";
  char len_buf[100];
  snprintf(len_buf, sizeof(len_buf), "Content-Length: %ld\r\n", end - start);
  const char *type;
  if (StringEndsWith(canon_path, ".htm") || StringEndsWith(canon_path, ".html"))
  {
    type = "Content-Type: text/html\r\n";
  }
  else
  {
    type = "Content-Type: application/octet-stream\r\n";
  }
  char range_buf[100];
  snprintf(range_buf, sizeof(range_buf), "Content-Range: bytes %ld-%ld/%ld\r\n", start, end - 1, st.st_size);

  const char *nl = "\r\n";
  WriteString(stream, head);
  WriteString(stream, len_buf);
  WriteString(stream, type);
  if (!range)
    WriteString(stream, acpt_rng);
  if (range)
    WriteString(stream, range_buf);
  WriteString(stream, nl);

  uint8_t buf[1024];
  int fd = open(canon_path, O_RDONLY);
  lseek(fd, start, SEEK_SET);
  size_t pos = start;
  while (pos < end)
  {
    size_t len = read(fd, buf, sizeof(buf));
    if (stream->write(stream, buf, len) == -1)
    {
      fprintf(stderr, "Failed to write socket: %s\n", strerror(errno));
      goto cleanup_fd;
    }
    pos += len;
  }

cleanup_fd:
  close(fd);
}

void *ConnectionHandler(void *arg)
{
  struct MyStream *stream = arg;

  bool has_range = false;
  struct RequestRange range;
  char* host;

  struct HttpRequest *req = ParseRequest(stream);
  if (!req)
  {
    fprintf(stderr, "Failed to parse request\n");
    goto cleanup_stream;
  }
  
  printf("Method: %s\n", req->method);
  printf("Uri: %s\n", req->uri);
  for (struct HttpHeader *cur = req->headers; cur; cur = cur->next)
  {
    printf("Header %s: %s\n", cur->name, cur->value);
    if (strcmp(cur->name, "Range") == 0)
    {
      has_range = ParseRangeHeader(cur->value, &range);
    }
    if (strcmp(cur->name, "Host") == 0)
    {
      host = cur->value;
    }
  }

  if (stream->userdata == NULL)
  {
    char new_url[MAX_LEN];
    snprintf(new_url, sizeof(new_url), "https://%s%s",host, req->uri);
    printf("Redirecting to %s", new_url);
    Send301(stream, new_url);
  }
  else
  {
    ServeFile(stream, req->uri, has_range ? &range : NULL);
  }
  FreeRequest(req);
cleanup_stream:
  stream->destroy(stream);
  return NULL;
}

void bind_http_port(int *sock_fd, struct sockaddr_in *sock_addr)
{
  *sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  sock_addr->sin_addr.s_addr = INADDR_ANY;
  sock_addr->sin_family = AF_INET;
  sock_addr->sin_port = htons(80);

  if ((bind(*sock_fd, (const struct sockaddr *)sock_addr, sizeof(*sock_addr))) != 0)
  {
    perror("http socket bind failed");
    exit(1);
  }
}

void bind_https_port(int *sock_fd, struct sockaddr_in *sock_addr)
{
  *sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  sock_addr->sin_addr.s_addr = INADDR_ANY;
  sock_addr->sin_family = AF_INET;
  sock_addr->sin_port = htons(443);

  if ((bind(*sock_fd, (const struct sockaddr *)sock_addr, sizeof(*sock_addr))) != 0)
  {
    perror("https socket bind failed");
    exit(1);
  }
}

void *handle_http_connection(void *sock_fd)
{
  socklen_t addr_size;
  int client_sock_fd;
  struct sockaddr_storage client_addr_storage;
  while (1)
  {
    addr_size = sizeof(client_addr_storage);
    client_sock_fd = accept(*(int *)sock_fd, (struct sockaddr *)&client_addr_storage,
                            &addr_size);
    if (client_sock_fd == -1)
    {
      perror("Unable to accept");
      exit(1);
    }
    char clientName[30];
    struct sockaddr_in *addr = (struct sockaddr_in *)&client_addr_storage;
    printf("Accepted connection from %s:%d\n", inet_ntop(AF_INET, &addr->sin_addr, clientName, sizeof(clientName)), addr->sin_port);

    struct MyStream *http_stream = InitTcpStream(client_sock_fd);

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, ConnectionHandler, http_stream))
    {
      perror("Could not create thread");
      exit(1);
    }
  }
}

void *handle_https_connection(void *sock_fd)
{

  socklen_t addr_size;
  int client_sock_fd;
  struct sockaddr_storage client_addr_storage;
  while (1)
  {
    addr_size = sizeof(client_addr_storage);
    client_sock_fd = accept(*(int *)sock_fd, (struct sockaddr *)&client_addr_storage,
                            &addr_size);
    if (client_sock_fd == -1)
    {
      perror("Unable to accept");
      exit(1);
    }
    char clientName[30];
    struct sockaddr_in *addr = (struct sockaddr_in *)&client_addr_storage;
    printf("Accepted connection from %s:%d\n", inet_ntop(AF_INET, &addr->sin_addr, clientName, sizeof(clientName)), addr->sin_port);

    struct MyStream *https_stream = InitTlsStream(client_sock_fd);

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, ConnectionHandler, https_stream))
    {
      perror("Could not create thread");
      exit(1);
    }
  }
}

int main()
{
  printf("Thank you for using tyhttpd!\n");
  signal(SIGPIPE, SIG_IGN);
  int http_server_sock_fd, https_server_sock_fd;
  struct sockaddr_in http_server_sock_addr, https_server_sock_addr;

  bind_http_port(&http_server_sock_fd, &http_server_sock_addr);
  bind_https_port(&https_server_sock_fd, &https_server_sock_addr);

  if (listen(http_server_sock_fd, 10) == -1 || listen(https_server_sock_fd, 10) == -1)
  {
    perror("Unable to listen");
    return 1;
  }

  pthread_t http_thread_id, https_thread_id;
  if (pthread_create(&http_thread_id, NULL, handle_http_connection, &http_server_sock_fd))
  {
    perror("Could not create thread");
    exit(1);
  }
  if (pthread_create(&https_thread_id, NULL, handle_https_connection, &https_server_sock_fd))
  {
    perror("Could not create thread");
    exit(1);
  }

  pthread_join(http_thread_id, NULL);
  pthread_join(https_thread_id, NULL);

  return 0;
}
