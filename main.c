#include <stdio.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/syslimits.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <signal.h>
#include "stream.h"
#include "http.h"
#include "utils.h"

void Send404(struct MyStream* stream) {
  const char * head = "HTTP/1.1 404 Not Found\r\n";
  const char * content = "The page you've requested could not be found.";
  char len_buf[100];
  snprintf(len_buf, sizeof(len_buf), "Content-Length: %lu\r\n", strlen(content));
  const char * type = "Content-Type: text/plain\r\n";
  const char * nl = "\r\n";
  WriteString(stream, head);
  WriteString(stream, len_buf);
  WriteString(stream, type);
  WriteString(stream, nl);
  WriteString(stream, content);
}

void Send400(struct MyStream* stream) {
  const char * head = "HTTP/1.1 400 Bad Request\r\n";
  const char * nl = "\r\n";
  WriteString(stream, head);
  WriteString(stream, nl);
}

void ServeFile(struct MyStream* stream, const char* uri, struct RequestRange* range) {
  if (strcmp(uri, "/") == 0) {
    uri = "/index.html";
  }
  char path[PATH_MAX];
  getcwd(path, sizeof(path));
  int cwd_len = strlen(path);
  if (UrlDecode(path + cwd_len, sizeof(path) - cwd_len - 1, uri, strlen(uri)) < 0) {
    fprintf(stderr, "Invalid URL encode\n");
    Send400(stream);
    return;
  }
  char canon_path[PATH_MAX];
  CanonicalPath(path, canon_path);
  if (strncmp(canon_path, path, cwd_len) != 0) {
    fprintf(stderr, "Trying to read file outside of current directory\n");
    Send400(stream);
    return;
  }

  struct stat st;
  if (stat(canon_path, &st) == -1) {
    fprintf(stderr, "Cannot open %s: %s\n", canon_path, strerror(errno));
    Send404(stream);
    return;
  }
  if (!(st.st_mode & S_IFREG)) {
    fprintf(stderr, "%s is not a regular file\n", canon_path);
    Send404(stream);
    return;
  }

  off_t start = range ? range->start : 0;
  off_t end = range ? (range->end + 1) : st.st_size;
  if (end > st.st_size) {
    end = st.st_size;
  }
  if (start >= end) {
    start = 0;
  }

  const char * head = range ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
  const char * acpt_rng = "Accept-Ranges: bytes\r\n";
  char len_buf[100];
  snprintf(len_buf, sizeof(len_buf), "Content-Length: %llu\r\n", end - start);
  const char * type;
  if (StringEndsWith(canon_path, ".htm") || StringEndsWith(canon_path, ".html")) {
    type = "Content-Type: text/html\r\n";
  } else {
    type = "Content-Type: application/octet-stream\r\n";
  }
  char range_buf[100];
  snprintf(range_buf, sizeof(range_buf), "Content-Range: bytes %llu-%llu/%llu\r\n", start, end - 1, st.st_size);

  const char * nl = "\r\n";
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
  while (pos < end) {
    size_t len = read(fd, buf, sizeof(buf));
    if (stream->write(stream, buf, len) == -1) {
      fprintf(stderr, "Failed to write socket: %s\n", strerror(errno));
      goto cleanup_fd;
    }
    pos += len;
  }

cleanup_fd:
  close(fd);
}

void* ConnectionHandler(void *arg) {
  struct MyStream *stream = arg;

  bool has_range = false;
  struct RequestRange range;

  struct HttpRequest *req = ParseRequest(stream);
  printf("Method: %s\n", req->method);
  printf("Uri: %s\n", req->uri);
  for (struct HttpHeader* cur = req->headers; cur; cur = cur->next) {
    printf("Header %s: %s\n", cur->name, cur->value);
    if (strcmp(cur->name, "Range") == 0) {
      has_range = ParseRangeHeader(cur->value, &range);
    }
  }

  ServeFile(stream, req->uri, has_range ? &range : NULL);
  FreeRequest(req);
  stream->destroy(stream);
  return NULL;
}

int main() {
  signal(SIGPIPE, SIG_IGN);
  int server_sock_fd, client_sock_fd;
  struct sockaddr_in server_sock_addr;
  struct sockaddr_storage client_addr_storage;
  socklen_t addr_size;

  server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  server_sock_addr.sin_addr.s_addr = INADDR_ANY;
  server_sock_addr.sin_family = AF_INET;
  server_sock_addr.sin_port = htons(8989);

  if ((bind(server_sock_fd, (const struct sockaddr *) &server_sock_addr, sizeof(server_sock_addr))) != 0) {
    perror("socket bind failed");
    return 1;
  }

  if (listen(server_sock_fd, 10) == -1) {
    perror("Unable to listen");
    return 1;
  }

  while (1) {
    addr_size = sizeof(client_addr_storage);
    client_sock_fd = accept(server_sock_fd,
                            (struct sockaddr *) &client_addr_storage,
                            &addr_size);
    if (client_sock_fd == -1) {
      perror("Unable to accept");
      return 1;
    }

    char clientName[30];

    printf("Accepted connection from %s\n", inet_ntop(AF_INET, &client_addr_storage,
                                                      clientName, sizeof(clientName)));

    struct MyStream *http_stream = InitTcpStream(client_sock_fd);

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, ConnectionHandler, http_stream)) {
      perror("Could not create thread");
      return 1;
    }
  }

  return 0;
}
