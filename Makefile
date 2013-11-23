MODULES = pg_web
OBJS = pg_web.o

EXTENSION 	= pg_web
EXTVERSION 	= $(shell grep default_version $(EXTENSION).control | \
			  sed -e "s/default_version[[:space:]]*=[[:space:]]*'\([^']*\)'/\1/")

DATA = pg_web--0.1.0.sql

PG_CONFIG	= pg_config

# verify version is 9.3 or later

PG93        = $(shell $(PG_CONFIG) --version | grep -qE " 8\.| 9\.0 | 9\.1| 9\.2" && echo no || echo yes)

ifeq ($(PG93),no)
$(error Requires PostgreSQL 9.3 or later)
endif

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)