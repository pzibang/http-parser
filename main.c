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

struct url_test {
  const char *name;
  const char *url;
  int is_connect;
  struct http_parser_url u;
  int rv;
};

static http_parser_settings settings_null =
{.on_message_begin = 0
,.on_header_field = 0
,.on_header_value = 0
,.on_url = 0
,.on_status = 0
,.on_body = 0
,.on_headers_complete = 0
,.on_message_complete = 0
,.on_chunk_header = 0
,.on_chunk_complete = 0
};

int my_url_callback(http_parser* parser, const char *at, size_t length) {
  /* access to thread local custom_data_t struct.
  Use this access save parsed data for later use into thread local
  buffer, or communicate over socket
  */
  parser->data;
  printf("my_url_callback\n");
  return 0;
}

int main(int argc, char** argv) 
{
  http_parser parser;
  http_parser_init(&parser, HTTP_REQUEST);
  size_t parsed;
  const char *buf;
  buf = "GET / HTTP/1.1\r\nheader: value\nhdr: value\r\n";
  settings_null.on_url = my_url_callback;
  parsed = http_parser_execute(&parser, &settings_null, buf, strlen(buf));
  assert(parsed == strlen(buf));

  assert(parser.nread == strlen(buf));  

  printf("%s\n", http_method_str(parser.method));
  printf("%s\n", http_status_str(parser.state));




  return 0;
}
