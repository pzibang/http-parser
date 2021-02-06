/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include "http_parser.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* rand */
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_HEADERS 13
#define MAX_ELEMENT_SIZE 2048
#define MAX_CHUNKS 16

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

struct message {
  const char *name; // for debugging purposes
  const char *raw;
  enum http_parser_type type;
  enum http_method method;
  int status_code;
  char response_status[MAX_ELEMENT_SIZE];
  char request_path[MAX_ELEMENT_SIZE];
  char request_url[MAX_ELEMENT_SIZE];
  char fragment[MAX_ELEMENT_SIZE];
  char query_string[MAX_ELEMENT_SIZE];
  char body[MAX_ELEMENT_SIZE];
  size_t body_size;
  const char *host;
  const char *userinfo;
  uint16_t port;
  int num_headers;
  enum { NONE=0, FIELD, VALUE } last_header_element;
  char headers [MAX_HEADERS][2][MAX_ELEMENT_SIZE];
  int should_keep_alive;

  int num_chunks;
  int num_chunks_complete;
  int chunk_lengths[MAX_CHUNKS];

  const char *upgrade; // upgraded body

  unsigned short http_major;
  unsigned short http_minor;
  uint64_t content_length;

  int message_begin_cb_called;
  int headers_complete_cb_called;
  int message_complete_cb_called;
  int status_cb_called;
  int message_complete_on_eof;
  int body_is_final;
  int allow_chunked_length;
};

static http_parser parser;
static int currently_parsing_eof;

static struct message messages[5];
static int num_messages;

size_t
strlncat(char *dst, size_t len, const char *src, size_t n)
{
  size_t slen;
  size_t dlen;
  size_t rlen;
  size_t ncpy;

  slen = strnlen(src, n);
  dlen = strnlen(dst, len);

  if (dlen < len) {
    rlen = len - dlen;
    ncpy = slen < rlen ? slen : (rlen - 1);
    memcpy(dst + dlen, src, ncpy);
    dst[dlen + ncpy] = '\0';
  }

  assert(len > slen + dlen);
  return slen + dlen;
}

void
check_body_is_final (const http_parser *p)
{
  if (messages[num_messages].body_is_final) {
    fprintf(stderr, "\n\n *** Error http_body_is_final() should return 1 "
                    "on last on_body callback call "
                    "but it doesn't! ***\n\n");
    assert(0);
    abort();
  }
  messages[num_messages].body_is_final = http_body_is_final(p);
}

int
message_begin_cb (http_parser *p)
{
  assert(p == &parser);
  assert(!messages[num_messages].message_begin_cb_called);
  messages[num_messages].message_begin_cb_called = TRUE;
  return 0;
}

int
header_field_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == &parser);
  struct message *m = &messages[num_messages];

  if (m->last_header_element != FIELD)
    m->num_headers++;

  strlncat(m->headers[m->num_headers-1][0],
           sizeof(m->headers[m->num_headers-1][0]),
           buf,
           len);

  m->last_header_element = FIELD;

  return 0;
}

int
header_value_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == &parser);
  struct message *m = &messages[num_messages];

  strlncat(m->headers[m->num_headers-1][1],
           sizeof(m->headers[m->num_headers-1][1]),
           buf,
           len);

  m->last_header_element = VALUE;

  return 0;
}

int
request_url_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == &parser);
  strlncat(messages[num_messages].request_url,
           sizeof(messages[num_messages].request_url),
           buf,
           len);
  return 0;
}

int
response_status_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == &parser);

  messages[num_messages].status_cb_called = TRUE;

  strlncat(messages[num_messages].response_status,
           sizeof(messages[num_messages].response_status),
           buf,
           len);
  return 0;
}

int
body_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == &parser);
  strlncat(messages[num_messages].body,
           sizeof(messages[num_messages].body),
           buf,
           len);
  messages[num_messages].body_size += len;
  check_body_is_final(p);
 // printf("body_cb: '%s'\n", requests[num_messages].body);
  return 0;
}

int
headers_complete_cb (http_parser *p)
{
  assert(p == &parser);
  messages[num_messages].method = parser.method;
  messages[num_messages].status_code = parser.status_code;
  messages[num_messages].http_major = parser.http_major;
  messages[num_messages].http_minor = parser.http_minor;
  messages[num_messages].content_length = parser.content_length;
  messages[num_messages].headers_complete_cb_called = TRUE;
  messages[num_messages].should_keep_alive = http_should_keep_alive(&parser);
  return 0;
}

int
message_complete_cb (http_parser *p)
{
  assert(p == &parser);
  if (messages[num_messages].should_keep_alive !=
      http_should_keep_alive(&parser))
  {
    fprintf(stderr, "\n\n *** Error http_should_keep_alive() should have same "
                    "value in both on_message_complete and on_headers_complete "
                    "but it doesn't! ***\n\n");
    assert(0);
    abort();
  }

  if (messages[num_messages].body_size &&
      http_body_is_final(p) &&
      !messages[num_messages].body_is_final)
  {
    fprintf(stderr, "\n\n *** Error http_body_is_final() should return 1 "
                    "on last on_body callback call "
                    "but it doesn't! ***\n\n");
    assert(0);
    abort();
  }

  messages[num_messages].message_complete_cb_called = TRUE;

  messages[num_messages].message_complete_on_eof = currently_parsing_eof;

  num_messages++;
  return 0;
}

int
chunk_header_cb (http_parser *p)
{
  assert(p == &parser);
  int chunk_idx = messages[num_messages].num_chunks;
  messages[num_messages].num_chunks++;
  if (chunk_idx < MAX_CHUNKS) {
    messages[num_messages].chunk_lengths[chunk_idx] = p->content_length;
  }

  return 0;
}

int
chunk_complete_cb (http_parser *p)
{
  assert(p == &parser);

  /* Here we want to verify that each chunk_header_cb is matched by a
   * chunk_complete_cb, so not only should the total number of calls to
   * both callbacks be the same, but they also should be interleaved
   * properly */
  assert(messages[num_messages].num_chunks ==
         messages[num_messages].num_chunks_complete + 1);

  messages[num_messages].num_chunks_complete++;
  return 0;
}

static http_parser_settings settings =
  {.on_message_begin = message_begin_cb
  ,.on_header_field = header_field_cb
  ,.on_header_value = header_value_cb
  ,.on_url = request_url_cb
  ,.on_status = response_status_cb
  ,.on_body = body_cb
  ,.on_headers_complete = headers_complete_cb
  ,.on_message_complete = message_complete_cb
  ,.on_chunk_header = chunk_header_cb
  ,.on_chunk_complete = chunk_complete_cb
  };

void
parser_init (enum http_parser_type type)
{
  num_messages = 0;
  http_parser_init(&parser, type);
  memset(&messages, 0, sizeof messages);
}

size_t parse (const char *buf, size_t len)
{
  size_t nparsed;
  currently_parsing_eof = (len == 0);
  nparsed = http_parser_execute(&parser, &settings, buf, len);

  printf("%s\n", http_method_str(parser.method));
  //show the version
  printf("%d.%d\n", parser.http_major, parser.http_minor);

  return nparsed;
}

void
test_simple (const char *buf, enum http_errno err_expected)
{
  test_simple_type(buf, err_expected, HTTP_REQUEST);
}

void
test_simple_type (const char *buf,
                  enum http_errno err_expected,
                  enum http_parser_type type)
{
  parser_init(type);

  enum http_errno err;

  parse(buf, strlen(buf));
  err = HTTP_PARSER_ERRNO(&parser);
  parse(NULL, 0);
  // printf("%s\n", http_errno_name(err));
  /* In strict mode, allow us to pass with an unexpected HPE_STRICT as
   * long as the caller isn't expecting success.
   */
#if HTTP_PARSER_STRICT
  if (err_expected != err && err_expected != HPE_OK && err != HPE_STRICT) {
#else
  if (err_expected != err) {
#endif
    fprintf(stderr, "\n*** test_simple expected %s, but saw %s ***\n\n%s\n",
        http_errno_name(err_expected), http_errno_name(err), buf);
    abort();
  }
}


int main(int argc, char** argv) 
{
  test_simple("GET / IHTTP/1.0\r\n\r\n", HPE_INVALID_CONSTANT);
  test_simple("GET / ICE/1.0\r\n\r\n", HPE_INVALID_CONSTANT);
  test_simple("GET / HTP/1.1\r\n\r\n", HPE_INVALID_VERSION);
  test_simple("GET / HTTP/01.1\r\n\r\n", HPE_INVALID_VERSION);
  test_simple("GET / HTTP/11.1\r\n\r\n", HPE_INVALID_VERSION);
  test_simple("GET / HTTP/1.01\r\n\r\n", HPE_INVALID_VERSION);

  test_simple("GET / HTTP/1.0\r\nHello: w\1rld\r\n\r\n", HPE_INVALID_HEADER_TOKEN);
  test_simple("GET / HTTP/1.0\r\nHello: woooo\2rld\r\n\r\n", HPE_INVALID_HEADER_TOKEN);

  // Extended characters - see nodejs/test/parallel/test-http-headers-obstext.js
  test_simple("GET / HTTP/1.1\r\n"
              "Test: Düsseldorf\r\n",
              HPE_OK);

  // Well-formed but incomplete
  test_simple("GET / HTTP/1.1\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: 6\r\n"
              "\r\n"
              "fooba",
              HPE_OK);

  // Unknown Transfer-Encoding in request
  test_simple("GET / HTTP/1.1\r\n"
              "Transfer-Encoding: unknown\r\n"
              "\r\n",
              HPE_INVALID_TRANSFER_ENCODING);



  return 0;
}
