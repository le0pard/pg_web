/*
 * web_server.c
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

#include <stdlib.h>
#include "dyad.h"

void start_server(void);

static void onData(dyad_Event *e) {
  dyad_write(e->stream, e->data, e->size);
}

static void onAccept(dyad_Event *e) {
  dyad_addListener(e->remote, DYAD_EVENT_DATA, onData, NULL);
  dyad_writef(e->remote, "Echo server\r\n");
}

void start_server(void) {
  dyad_init();

  dyad_Stream *serv = dyad_newStream();
  dyad_addListener(serv, DYAD_EVENT_ACCEPT, onAccept, NULL);
  dyad_listen(serv, 8000);

  /*
  while (dyad_getStreamCount() > 0) {
    dyad_update();
  }
  
  dyad_shutdown();
  return 0;
  */
}