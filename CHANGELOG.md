## master

* release

PG_CPPFLAGS = -I$(libpq_srcdir)
SHLIB_LINK = $(libpq)

#include "libpq-fe.h"

const char *conninfo = "dbname = postgres";
PGconn     *conn;
PGresult   *res;
int32       count = 0;

/* Make a connection to the database */
conn = PQconnectdb(conninfo);

/* Check to see that the backend connection was successfully made */
if (PQstatus(conn) == CONNECTION_OK)
{
  /* Start a transaction block */
  res = PQexec(conn, "BEGIN");
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
      PQclear(res);

      res = PQexec(conn, "SELECT COUNT(*) FROM pg_class;");
      if (PQresultStatus(res) == PGRES_TUPLES_OK)
      {
          count = atoi(PQgetvalue(res,0,0));

          PQclear(res);

          /* end the transaction */
          res = PQexec(conn, "END");
          PQclear(res);

          /* close the connection to the database and cleanup */
          PQfinish(conn);

      } else {
        ereport( INFO, (errmsg( "SELECT error %s\n", PQerrorMessage(conn) )));

        PQclear(res);
        /* close the connection to the database and cleanup */
        PQfinish(conn);
      }

  } else {
    ereport( INFO, (errmsg( "BEGIN error %s\n", PQerrorMessage(conn) )));

    PQclear(res);
    /* close the connection to the database and cleanup */
    PQfinish(conn);
  }

}