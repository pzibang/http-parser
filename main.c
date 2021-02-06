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

const struct url_test url_tests[] =
{ {.name="proxy request"
  ,.url="http://hostname/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << UF_SCHEMA) | (1 << UF_HOST) | (1 << UF_PATH)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  7,  8 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 15,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="proxy request with port"
  ,.url="http://hostname:444/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << UF_SCHEMA) | (1 << UF_HOST) | (1 << UF_PORT) | (1 << UF_PATH)
    ,.port=444
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  7,  8 } /* UF_HOST */
      ,{ 16,  3 } /* UF_PORT */
      ,{ 19,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="CONNECT request"
  ,.url="hostname:443"
  ,.is_connect=1
  ,.u=
    {.field_set=(1 << UF_HOST) | (1 << UF_PORT)
    ,.port=443
    ,.field_data=
      {{  0,  0 } /* UF_SCHEMA */
      ,{  0,  8 } /* UF_HOST */
      ,{  9,  3 } /* UF_PORT */
      ,{  0,  0 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="CONNECT request but not connect"
  ,.url="hostname:443"
  ,.is_connect=0
  ,.rv=1
  }

, {.name="proxy ipv6 request"
  ,.url="http://[1:2::3:4]/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << UF_SCHEMA) | (1 << UF_HOST) | (1 << UF_PATH)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  8,  8 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 17,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="proxy ipv6 request with port"
  ,.url="http://[1:2::3:4]:67/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << UF_SCHEMA) | (1 << UF_HOST) | (1 << UF_PORT) | (1 << UF_PATH)
    ,.port=67
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  8,  8 } /* UF_HOST */
      ,{ 18,  2 } /* UF_PORT */
      ,{ 20,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="CONNECT ipv6 address"
  ,.url="[1:2::3:4]:443"
  ,.is_connect=1
  ,.u=
    {.field_set=(1 << UF_HOST) | (1 << UF_PORT)
    ,.port=443
    ,.field_data=
      {{  0,  0 } /* UF_SCHEMA */
      ,{  1,  8 } /* UF_HOST */
      ,{ 11,  3 } /* UF_PORT */
      ,{  0,  0 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="ipv4 in ipv6 address"
  ,.url="http://[2001:0000:0000:0000:0000:0000:1.9.1.1]/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << UF_SCHEMA) | (1 << UF_HOST) | (1 << UF_PATH)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  8, 37 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 46,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="extra ? in query string"
  ,.url="http://a.tbcdn.cn/p/fp/2010c/??fp-header-min.css,fp-base-min.css,"
  "fp-channel-min.css,fp-product-min.css,fp-mall-min.css,fp-category-min.css,"
  "fp-sub-min.css,fp-gdp4p-min.css,fp-css3-min.css,fp-misc-min.css?t=20101022.css"
  ,.is_connect=0
  ,.u=
    {.field_set=(1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PATH) | (1<<UF_QUERY)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  7, 10 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 17, 12 } /* UF_PATH */
      ,{ 30,187 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="space URL encoded"
  ,.url="/toto.html?toto=a%20b"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_PATH) | (1<<UF_QUERY)
    ,.port=0
    ,.field_data=
      {{  0,  0 } /* UF_SCHEMA */
      ,{  0,  0 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{  0, 10 } /* UF_PATH */
      ,{ 11, 10 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }


, {.name="URL fragment"
  ,.url="/toto.html#titi"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_PATH) | (1<<UF_FRAGMENT)
    ,.port=0
    ,.field_data=
      {{  0,  0 } /* UF_SCHEMA */
      ,{  0,  0 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{  0, 10 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{ 11,  4 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="complex URL fragment"
  ,.url="http://www.webmasterworld.com/r.cgi?f=21&d=8405&url="
    "http://www.example.com/index.html?foo=bar&hello=world#midpage"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PATH) | (1<<UF_QUERY) |\
      (1<<UF_FRAGMENT)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  7, 22 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 29,  6 } /* UF_PATH */
      ,{ 36, 69 } /* UF_QUERY */
      ,{106,  7 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="complex URL from node js url parser doc"
  ,.url="http://host.com:8080/p/a/t/h?query=string#hash"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PORT) | (1<<UF_PATH) |\
      (1<<UF_QUERY) | (1<<UF_FRAGMENT)
    ,.port=8080
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  7,  8 } /* UF_HOST */
      ,{ 16,  4 } /* UF_PORT */
      ,{ 20,  8 } /* UF_PATH */
      ,{ 29, 12 } /* UF_QUERY */
      ,{ 42,  4 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="complex URL with basic auth from node js url parser doc"
  ,.url="http://a:b@host.com:8080/p/a/t/h?query=string#hash"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PORT) | (1<<UF_PATH) |\
      (1<<UF_QUERY) | (1<<UF_FRAGMENT) | (1<<UF_USERINFO)
    ,.port=8080
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{ 11,  8 } /* UF_HOST */
      ,{ 20,  4 } /* UF_PORT */
      ,{ 24,  8 } /* UF_PATH */
      ,{ 33, 12 } /* UF_QUERY */
      ,{ 46,  4 } /* UF_FRAGMENT */
      ,{  7,  3 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="double @"
  ,.url="http://a:b@@hostname:443/"
  ,.is_connect=0
  ,.rv=1
  }

, {.name="proxy empty host"
  ,.url="http://:443/"
  ,.is_connect=0
  ,.rv=1
  }

, {.name="proxy empty port"
  ,.url="http://hostname:/"
  ,.is_connect=0
  ,.rv=1
  }

, {.name="CONNECT with basic auth"
  ,.url="a:b@hostname:443"
  ,.is_connect=1
  ,.rv=1
  }

, {.name="CONNECT empty host"
  ,.url=":443"
  ,.is_connect=1
  ,.rv=1
  }

, {.name="CONNECT empty port"
  ,.url="hostname:"
  ,.is_connect=1
  ,.rv=1
  }

, {.name="CONNECT with extra bits"
  ,.url="hostname:443/"
  ,.is_connect=1
  ,.rv=1
  }

, {.name="space in URL"
  ,.url="/foo bar/"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy basic auth with space url encoded"
  ,.url="http://a%20:b@host.com/"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PATH) | (1<<UF_USERINFO)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{ 14,  8 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 22,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  7,  6 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="carriage return in URL"
  ,.url="/foo\rbar/"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy double : in URL"
  ,.url="http://hostname::443/"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy basic auth with double :"
  ,.url="http://a::b@host.com/"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PATH) | (1<<UF_USERINFO)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{ 12,  8 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 20,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  7,  4 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="line feed in URL"
  ,.url="/foo\nbar/"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy empty basic auth"
  ,.url="http://@hostname/fo"
  ,.u=
    {.field_set= (1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PATH)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  8,  8 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 16,  3 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }
, {.name="proxy line feed in hostname"
  ,.url="http://host\name/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy % in hostname"
  ,.url="http://host%name/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy ; in hostname"
  ,.url="http://host;ame/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy basic auth with unreservedchars"
  ,.url="http://a!;-_!=+$@host.com/"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PATH) | (1<<UF_USERINFO)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{ 17,  8 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 25,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  7,  9 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="proxy only empty basic auth"
  ,.url="http://@/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy only basic auth"
  ,.url="http://toto@/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy emtpy hostname"
  ,.url="http:///fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy = in URL"
  ,.url="http://host=ame/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="ipv6 address with Zone ID"
  ,.url="http://[fe80::a%25eth0]/"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PATH)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  8, 14 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 23,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="ipv6 address with Zone ID, but '%' is not percent-encoded"
  ,.url="http://[fe80::a%eth0]/"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PATH)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* UF_SCHEMA */
      ,{  8, 12 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{ 21,  1 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="ipv6 address ending with '%'"
  ,.url="http://[fe80::a%]/"
  ,.rv=1 /* s_dead */
  }

, {.name="ipv6 address with Zone ID including bad character"
  ,.url="http://[fe80::a%$HOME]/"
  ,.rv=1 /* s_dead */
  }

, {.name="just ipv6 Zone ID"
  ,.url="http://[%eth0]/"
  ,.rv=1 /* s_dead */
  }

, {.name="empty url"
  ,.url=""
  ,.is_connect=0
  ,.rv=1
  }

, {.name="NULL url"
  ,.url=NULL
  ,.is_connect=0
  ,.rv=1
  }

, {.name="full of spaces url"
  ,.url="  "
  ,.is_connect=0
  ,.rv=1
  }

#if HTTP_PARSER_STRICT

, {.name="tab in URL"
  ,.url="/foo\tbar/"
  ,.rv=1 /* s_dead */
  }

, {.name="form feed in URL"
  ,.url="/foo\fbar/"
  ,.rv=1 /* s_dead */
  }

#else /* !HTTP_PARSER_STRICT */

, {.name="tab in URL"
  ,.url="/foo\tbar/"
  ,.u=
    {.field_set=(1 << UF_PATH)
    ,.field_data=
      {{  0,  0 } /* UF_SCHEMA */
      ,{  0,  0 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{  0,  9 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }

, {.name="form feed in URL"
  ,.url="/foo\fbar/"
  ,.u=
    {.field_set=(1 << UF_PATH)
    ,.field_data=
      {{  0,  0 } /* UF_SCHEMA */
      ,{  0,  0 } /* UF_HOST */
      ,{  0,  0 } /* UF_PORT */
      ,{  0,  9 } /* UF_PATH */
      ,{  0,  0 } /* UF_QUERY */
      ,{  0,  0 } /* UF_FRAGMENT */
      ,{  0,  0 } /* UF_USERINFO */
      }
    }
  ,.rv=0
  }
#endif
};

void
dump_url (const char *url, const struct http_parser_url *u)
{
  unsigned int i;

  printf("\tfield_set: 0x%x, port: %u\n", u->field_set, u->port);
  for (i = 0; i < UF_MAX; i++) 
  {
    if ((u->field_set & (1 << i)) == 0) 
    {
      printf("\tfield_data[%u]: unset\n", i);
      continue;
    }

    printf("\tfield_data[%u]: off: %u len: %u part: \"%.*s\n\"",
           i,
           u->field_data[i].off,
           u->field_data[i].len,
           u->field_data[i].len,
           url + u->field_data[i].off);
  }
}

int main(int argc, char** argv) {
  
  char my_data[] = "application-specific data";
  http_parser parser;
  parser.data = my_data;
  http_parser_init(&parser, HTTP_REQUEST);

  if (parser.data != my_data) {
    printf("\n*** parser.data not preserved accross http_parser_init ***\n\n");
    abort();
  }


  struct http_parser_url u;
  const struct url_test *test;
  unsigned int i;
  int rv;

  for (i = 0; i < (sizeof(url_tests) / sizeof(url_tests[0])); i++) 
  {
    test = &url_tests[i];
    memset(&u, 0, sizeof(u));

    rv = http_parser_parse_url(test->url,
                               test->url ? strlen(test->url) : 0,
                               test->is_connect,
                               &u);

    if (test->rv == 0) 
    {
      if (rv != 0) 
      {
        printf("\n*** http_parser_parse_url(\"%s\") \"%s\" test failed, "
               "unexpected rv %d ***\n\n", test->url, test->name, rv);
        abort();
      }

      printf("i = %d :port: %d;field_data len:%d,off:%d\n ", i, u.port, u.field_data->len, u.field_data->off);

      if (memcmp(&u, &test->u, sizeof(u)) != 0)
      {
        printf("\n*** http_parser_parse_url(\"%s\") \"%s\" failed ***\n",
               test->url, test->name);

        printf("target http_parser_url:\n");
        dump_url(test->url, &test->u);
        printf("result http_parser_url:\n");
        dump_url(test->url, &u);

        abort();
      }
    } 
    else 
    {
      /* test->rv != 0 */
      if (rv == 0) 
      {
        printf("\n*** http_parser_parse_url(\"%s\") \"%s\" test failed, "
               "unexpected rv %d ***\n\n", test->url, test->name, rv);
        abort();
      }
    }
  }



  return 0;
}
