EXTENSION    = $(shell grep -m 1 '"name":' META.json | \
				sed -e 's/[[:space:]]*"name":[[:space:]]*"\([^"]*\)",/\1/')
EXTVERSION   = $(shell grep default_version $(EXTENSION).control | \
				sed -e "s/default_version[[:space:]]*=[[:space:]]*'\([^']*\)'/\1/")

#SHLIB_LINK   = -ldl -lpthread
PG_CPPFLAGS = -I$(libpq_srcdir)
SHLIB_LINK = $(libpq)

DATA         = $(filter-out $(wildcard sql/*--*.sql),$(wildcard sql/*.sql))
TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test
DOCS         = $(wildcard doc/*.md)
# use module big instead:
#MODULES                = $(patsubst %.c,%,$(wildcard src/*.c))
MODULE_big   = $(EXTENSION)
OBJS         = $(patsubst %.c,%.o,$(wildcard src/*.c))
PG_CONFIG    = pg_config
PG91         = $(shell $(PG_CONFIG) --version | grep -qE " 8\.| 9\.0 | 9\.1| 9\.2" && echo no || echo yes)

ifeq ($(PG93),no)
$(error Requires PostgreSQL 9.3 or later)
endif

all: sql/$(EXTENSION)--$(EXTVERSION).sql

sql/$(EXTENSION)--$(EXTVERSION).sql: sql/$(EXTENSION).sql
				cp $< $@

DATA = $(wildcard sql/*--*.sql) sql/$(EXTENSION)--$(EXTVERSION).sql
EXTRA_CLEAN = sql/$(EXTENSION)--$(EXTVERSION).sql

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)


dist:
				git archive --format zip --prefix=$(EXTENSION)-$(EXTVERSION)/ -o $(EXTENSION)-$(EXTVERSION).zip HEAD