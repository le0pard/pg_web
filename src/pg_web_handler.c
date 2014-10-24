/*
 * pg_web_handler.c
 *
 * PostgreSQL extension with web interface
 *
 *
 * Written by Alexey Vasiliev
 * leopard.not.a@gmail.com
 *
 * Copyright 2013 Alexey Vasiliev. This program is Free
 * Software; see the LICENSE file for the license conditions.
 */

#include "pg_web_handler.h"

static int count = 0;

void onWebLine(dyad_Event *e) {
  char path[128];
  if (sscanf(e->data, "GET %127s", path) == 1) {
    /* Print request */
    printf("%s %s\n", dyad_getAddress(e->stream), path);
    elog(LOG, "Hello from pg_web! By I should be a HTTP server."); /* Say Hello to the world */
    /* Send header */
    dyad_writef(e->stream, "HTTP/1.1 200 OK\r\n");
    dyad_writef(e->stream, "Content-Type:text/html; charset=utf-8\r\n");
    dyad_writef(e->stream, "\r\n");
    /* Handle request */
    if (!strcmp(path, "/")) {
      dyad_writef(e->stream, "<html><body><pre>"
                             "<a href='/date'>date</a><br>"
                             "<a href='/count'>count</a><br>"
                             "<a href='/ip'>ip</a>"
                             "</pre></body></html>" );

    } else if (!strcmp(path, "/date")) {
      time_t t = time(0);
      dyad_writef(e->stream, "%s", ctime(&t));

    } else if (!strcmp(path, "/count")) {
      dyad_writef(e->stream, "%d", ++count);

    } else if (!strcmp(path, "/ip")) {
      dyad_writef(e->stream, "%s", dyad_getAddress(e->stream));

    } else {
      dyad_writef(e->stream, "bad request '%s'", path);
    }
    /* Close stream when all data has been sent */
    dyad_end(e->stream);
  }
}

void onWebAccept(dyad_Event *e) {
  dyad_addListener(e->remote, DYAD_EVENT_LINE, onWebLine, NULL);
}

void onWebListen(dyad_Event *e) {
  elog(LOG, "server listening: http://localhost:%d\n", dyad_getPort(e->stream));
}

void onWebError(dyad_Event *e) {
  elog(LOG, "server error: %s\n", e->msg);
}