/*
 ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * This file is a modified version of the lwIP web server demo. The original
 * author is unknown because the file didn't contain any license information.
 */

/**
 * @file web.c
 * @brief HTTP server wrapper thread code.
 * @addtogroup WEB_THREAD
 * @{
 */

#include <string.h>
#include "ch.h"

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "web.h"
#include "fatfsWrapper.h"

#if LWIP_NETCONN

static const char http_html_hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
static const char http_index_html[] = "<html><head><title>Congrats!</title></head><body><h1>Welcome to Lightwallcontroller</h1><p>This is a small test page.<ul>";
static const char http_index_html1[] = "</ul></body></html>";
static const char http_index_html2[] = "</ul><p>Your sended:<ul>";
static const char list_start[] = "<li>";
static const char list_end[] = "</li>";

static void http_server_serve(struct netconn *conn)
  {
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;

    FRESULT res;
    FILINFO fno;
    DIR dir;
    char *fn;

#if _USE_LFN
    static char lfn[_MAX_LFN + 1];
    fno.lfname = lfn;
    fno.lfsize = sizeof lfn;
#endif

    /* Read the data from the port, blocking if nothing yet there.
     We assume the request (the part we care about) is in one netbuf */
    err = netconn_recv(conn, &inbuf);

    if (err == ERR_OK)
      {
        netbuf_data(inbuf, (void **)&buf, &buflen);

        /* Is this an HTTP GET command? (only check the first 5 chars, since
         there are other formats for GET, and we're keeping it very simple )*/
        if (buflen>=5 &&
            buf[0]=='G' &&
            buf[1]=='E' &&
            buf[2]=='T' &&
            buf[3]==' ' &&
            buf[4]=='/' )
          {

            /* Send the HTML header
             * subtract 1 from the size, since we don't send the \0 in the string
             * NETCONN_NOCOPY: our data is const static, so no need to copy it
             */
            netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);

            /* Send our HTML page */
            netconn_write(conn, http_index_html, sizeof(http_index_html)-1, NETCONN_NOCOPY);

            res = wf_opendir(&dir, "/");
            if (res == FR_OK)
              {
                for (;;)
                  {
                    res = wf_readdir(&dir, &fno);
                    if (res != FR_OK || fno.fname[0] == 0)
                    break;
                    if (fno.fname[0] == '.')
                    continue;
#if _USE_LFN
                    fn = *fno.lfname ? fno.lfname : fno.fname;
#else
                    fn = fno.fname;
#endif
                    netconn_write(conn, list_start, sizeof(list_start) -1, NETCONN_NOCOPY);
                    netconn_write(conn, fn, strlen(fn), NETCONN_COPY);
                    netconn_write(conn, list_end, sizeof(list_end) -1, NETCONN_NOCOPY);
                  }
              }
            netconn_write(conn, http_index_html2, sizeof(http_index_html1)-1, NETCONN_NOCOPY);
            netconn_write(conn, list_start, sizeof(list_start) -1, NETCONN_NOCOPY);
			netconn_write(conn, buf, buflen, NETCONN_COPY); /* Send back the request */
			netconn_write(conn, list_end, sizeof(list_end) -1, NETCONN_NOCOPY);

            netconn_write(conn, http_index_html1, sizeof(http_index_html1)-1, NETCONN_NOCOPY); /*footer of html */
          }
      }
    /* Close the connection (server closes in HTTP) */
    netconn_close(conn);

    /* Delete the buffer (netconn_recv gives us ownership,
     so we have to make sure to deallocate the buffer) */
    netbuf_delete(inbuf);
  }

/**
 * Stack area for the http thread.
 */
WORKING_AREA(wa_http_server, WEB_THREAD_STACK_SIZE);

/**
 * HTTP server thread.
 */
msg_t http_server(void *p)
  {
    struct netconn *conn, *newconn;
    err_t err;
    chRegSetThreadName("httpd");
    (void)p;

    /* Create a new TCP connection handle */
    conn = netconn_new(NETCONN_TCP);
    LWIP_ERROR("http_server: invalid conn", (conn != NULL), return RDY_RESET;);

    /* Bind to port 80 (HTTP) with default IP address */
    netconn_bind(conn, NULL, WEB_THREAD_PORT);

    /* Put the connection into LISTEN state */
    netconn_listen(conn);

    /* Goes to the final priority after initialization.*/
    chThdSetPriority(WEB_THREAD_PRIORITY);

    while(1)
      {
        err = netconn_accept(conn, &newconn);
        if (err != ERR_OK)
        continue;
        http_server_serve(newconn);
        netconn_delete(newconn);
      }
    return RDY_OK;
  }

#endif /* LWIP_NETCONN */

/** @} */
