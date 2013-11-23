/*
 * config_log.c
 *
 * PostgreSQL extension with web interface
 *
 *
 * Written by Alexey Vasiliev
 * leopard.not.a@gmail.com
 *
 * Copyright 2013 Alexey Vasiliev. This program is Free
 * Software; see the README.md file for the license conditions.
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
#include "tcop/utility.h"

/* Essential for shared libs! */
PG_MODULE_MAGIC;

/* Entry point of library loading */
void _PG_init(void);

/* flags set by signal handlers */
static volatile sig_atomic_t got_sigterm = false;

/* GUC variables */
/*static bool pg_web_enabled = true;*/

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
                        proc_exit(1);

                elog(LOG, "Hello World!"); /* Say Hello to the world */
        }
        proc_exit(0);
}

/*
 * Entrypoint of this module.
 */
void
_PG_init(void)
{
	BackgroundWorker	worker;

	/* get GUC settings, if available */
  /*
	DefineCustomStringVariable(
      "pg_web.enabled",
      "Is pg_web enabled",
      "pg_web is enabled (default: true).",
      &pg_web_enabled,
      true,
      PGC_POSTMASTER,
      0,
      NULL,
      NULL,
      NULL
    );
  */
	/* register the worker processes */
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
	worker.bgw_main = pg_web_main;

	worker.bgw_restart_time = 1;
	worker.bgw_main_arg = (Datum) 0;

	/* this value is shown in the process list */
	snprintf(worker.bgw_name, BGW_MAXLEN, "pg_web");

	RegisterBackgroundWorker(&worker);
}

