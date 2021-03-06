/*
 * Copyright (C) 2012 Marian Marinov <mm@yuhu.biz>
 * 
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <unistd.h>		/* needed for the execve */
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_log.h"
#include "http_main.h"
#include "util_script.h"
#include "scoreboard.h"

#define MODULE_NAME "mod_ipblock"
#define MODULE_VERSION "0.2"

#ifndef APACHE_RELEASE
#define APACHE2
#endif

#ifdef APACHE2
#include "http_connection.h"
#include "apr_strings.h"
#include "ap_mpm.h"
#endif

#define MAX_CMD 350			/* maximum size of the path to the command */

typedef struct {
    signed int limit;       /* max number of connections per IP */
    char cmd[MAX_CMD];      /* cmd to execute when the limit is reached*/
} ipblock_config;

#ifdef APACHE2
static int server_limit, thread_limit;
#endif
