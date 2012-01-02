#include <unistd.h>		/* needed for the execve */
#include <stdio.h>		/* needed for the sprintf */
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
