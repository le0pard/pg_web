/*
 * pg_web.c
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

#include "postgres.h"

/* Following are required for all bgworker */
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"

/* these headers are used by this particular worker's code */
#include "access/xact.h"
#include "executor/spi.h"
#include "fmgr.h"
#include "lib/stringinfo.h"
#include "pgstat.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"

/* web server */
#include "dyad.h"

/* Essential for shared libs! */
PG_MODULE_MAGIC;

/* Entry point of library loading */
void _PG_init(void);

/* flags set by signal handlers */
static volatile sig_atomic_t got_sigterm = false;

/* GUC variables */
static int pg_web_setting_port; //http port int
static char pg_web_setting_port_str[5]; //http port str

/* web server */

static int count = 0;

static void onLine(dyad_Event *e) {
  char path[128];
  if (sscanf(e->data, "GET %127s", path) == 1) {
    /* Print request */
    printf("%s %s\n", dyad_getAddress(e->stream), path);
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
                             "</pre></html></body>" );

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

static void onAccept(dyad_Event *e) {
  dyad_addListener(e->remote, DYAD_EVENT_LINE, onLine, NULL);
}

static void onListen(dyad_Event *e) {
  printf("server listening: http://localhost:%d\n", dyad_getPort(e->stream));
}

static void onError(dyad_Event *e) {
  printf("server error: %s\n", e->msg);
}

/*
 * pg_web_sigterm
 *
 * SIGTERM handler.
 */
static void
pg_web_sigterm(SIGNAL_ARGS)
{
  int save_errno = errno;
  got_sigterm = true;
  if (MyProc)
    SetLatch(&MyProc->procLatch);
  errno = save_errno;
}

/*
 * pg_web_exit
 *
 * SIGTERM handler.
 */
static void
pg_web_exit(int code)
{
  dyad_shutdown();
  proc_exit(code);
}


/*
 * pg_web_main
 *
 * Main loop processing.
 */
static void
pg_web_main(Datum main_arg)
{

  /* Set up the sigterm signal before unblocking them */
  pqsignal(SIGTERM, pg_web_sigterm);

  /* We're now ready to receive signals */
  BackgroundWorkerUnblockSignals();

  /* Connect to our database */
  BackgroundWorkerInitializeConnection("postgres", NULL);

  ereport( INFO, (errmsg( "Start web server on port %s\n", pg_web_setting_port_str )));
  
  dyad_Stream *s;
  dyad_init();

  s = dyad_newStream();
  dyad_addListener(s, DYAD_EVENT_ERROR,  onError,  NULL);
  dyad_addListener(s, DYAD_EVENT_ACCEPT, onAccept, NULL);
  dyad_addListener(s, DYAD_EVENT_LISTEN, onListen, NULL);
  dyad_listen(s, pg_web_setting_port);

  /* begin loop */
  while (!got_sigterm)
  {
    //int rc;
    
    dyad_update();

    /* Wait 10s */
    /*
    rc = WaitLatch(&MyProc->procLatch,
      WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
      10000L);
    ResetLatch(&MyProc->procLatch);
    */
    /* Emergency bailout if postmaster has died */
    /*
    if (rc & WL_POSTMASTER_DEATH)
      pg_web_exit(1);
    */
    /*
    if (WL_POSTMASTER_DEATH)
      pg_web_exit(1);
    */
    elog(LOG, "Hello from pg_web! By I should be a HTTP server."); /* Say Hello to the world */
  }
  
  pg_web_exit(0);
}

/*
 * pg_web_setting_port_hook
 *
 * Hook for http setting port
 */
static void pg_web_setting_port_hook(int newval, void *extra)
{
  sprintf(pg_web_setting_port_str, "%d", newval);
}

/*
 * Entrypoint of this module.
 */
void
_PG_init(void)
{
  BackgroundWorker	worker;

  /* get GUC settings, if available */

  DefineCustomIntVariable(
    "pg_web.port",
    "HTTP port for pg_web",
    "HTTP port for pg_web (default: 8080).",
    &pg_web_setting_port,
    8080,
    10,
    65000,
    PGC_POSTMASTER,
    0,
    NULL,
    pg_web_setting_port_hook,
    NULL
  );

  /* register the worker processes */
  worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
  worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
  worker.bgw_main = pg_web_main;
  /* Wait 1 seconds for restart before crash */
  worker.bgw_restart_time = 1;
  worker.bgw_main_arg = (Datum) 0;

  /* this value is shown in the process list */
  snprintf(worker.bgw_name, BGW_MAXLEN, "pg_web");

  RegisterBackgroundWorker(&worker);
}

