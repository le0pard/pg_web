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
#include "pg_web_sql.h"
#include "pg_web_server.h"

/* Essential for shared libs! */
PG_MODULE_MAGIC;

/* Entry point of library loading */
void _PG_init(void);

/* flags set by signal handlers */
static volatile sig_atomic_t got_sigterm = false;

/* GUC variables */
static int pg_web_setting_port; //http port int
static char pg_web_setting_port_str[5]; //http port str

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

  /* begin loop */
  while (!got_sigterm)
  {
    int rc;

    /* Wait 10s */
    rc = WaitLatch(&MyProc->procLatch,
      WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
      10000L);
    ResetLatch(&MyProc->procLatch);

    /* Emergency bailout if postmaster has died */
    if (rc & WL_POSTMASTER_DEATH)
      pg_web_exit(1);

    elog(LOG, "Hello World from pg_web!"); /* Say Hello to the world */
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

