/*
 * pg_web_handler.h
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

#include <stdio.h>
#include <time.h>
#include "postgres.h"
#include "dyad.h"

void onWebLine(dyad_Event *e);
void onWebAccept(dyad_Event *e);
void onWebListen(dyad_Event *e);
void onWebError(dyad_Event *e);