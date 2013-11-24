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
#include "tcop/utility.h"

/* web server */
#include <stdio.h>
#include <string.h>
#include "mongoose.h"

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
static struct mg_context *mongoose_ctx;
static struct mg_callbacks mongoose_callbacks;

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
  mg_stop(mongoose_ctx);
  proc_exit(code);
}

/*
 * begin_request_handler
 *
 * This function will be called by mongoose on every new request.
 */
static int begin_request_handler(struct mg_connection *conn) {
  const struct mg_request_info *request_info = mg_get_request_info(conn);
  char content[100];

  // Prepare the message we're going to send
  int content_length = snprintf(content, sizeof(content),
                                "Hello from pg_web! Remote port: %d",
                                request_info->remote_port);

  // Send HTTP reply to the client
  mg_printf(conn,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"        // Always set Content-Length
            "\r\n"
            "%s",
            content_length, content);

  // Returning non-zero tells mongoose that our function has replied to
  // the client, and mongoose should not send client any more data.
  return 1;
}


/*
 * pg_web_main
 *
 * Main loop processing.
 */
static void
pg_web_main(Datum main_arg)
{
        // List of options. Last element must be NULL.
        const char *options[] = {"listening_ports", pg_web_setting_port_str, NULL};

        /* Set up the sigterm signal before unblocking them */
        pqsignal(SIGTERM, pg_web_sigterm);

        /* We're now ready to receive signals */
        BackgroundWorkerUnblockSignals();

        // Prepare callbacks structure. We have only one callback, the rest are NULL.
        memset(&mongoose_callbacks, 0, sizeof(mongoose_callbacks));
        mongoose_callbacks.begin_request = begin_request_handler;

        // Start the web server.
        mongoose_ctx = mg_start(&mongoose_callbacks, NULL, options);

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

                /*elog(LOG, "Hello World!");*/ /* Say Hello to the world */
        }
        pg_web_exit(0);
}

/*
 * pg_web_setting_port_hook
 *
 * Hook for setting port
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

