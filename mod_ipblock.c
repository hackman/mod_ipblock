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

#include "mod_ipblock.h"

#ifdef APACHE2
module AP_MODULE_DECLARE_DATA ipblock_module;
#else
module MODULE_VAR_EXPORT ipblock_module;
#endif

/* Create per-server configuration structure. Used by the quick handler. */
#ifdef APACHE2
static void *ipblock_create_config(apr_pool_t *p, server_rec *s) {
	ipblock_config *cfg = (ipblock_config *) apr_pcalloc(p, sizeof(ipblock_config));
	ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, 
		MODULE_NAME " " MODULE_VERSION " started.");
	ap_mpm_query(AP_MPMQ_HARD_LIMIT_THREADS, &thread_limit);
	ap_mpm_query(AP_MPMQ_HARD_LIMIT_DAEMONS, &server_limit);
#else
static void *ipblock_create_config(pool *p, server_rec *s)  {
	ipblock_config *cfg = (ipblock_config *) ap_pcalloc(p, sizeof(ipblock_config));
#endif
	cfg->limit = 0;
	memset(&cfg->cmd, 0, MAX_CMD);
	return cfg;
}

/* Parse the IPBlockLimit directive */
static const char *ipblock_limit_cmd(cmd_parms *cmd, void *mconfig, const char *arg) {
    ipblock_config *cfg = (ipblock_config *) ap_get_module_config(cmd->server->module_config, &ipblock_module);
	cfg->limit = atoi(arg);
    return NULL;
}

/* Parse the IPBlockCmd directive */
static const char *ipblock_parse_cmd(cmd_parms *cmd, void *mconfig, const char *arg) {
    ipblock_config *cfg = (ipblock_config *) ap_get_module_config(cmd->server->module_config, &ipblock_module);
	if (strlen(arg) >= MAX_CMD) {
#ifdef APACHE2
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, NULL, "command too long");
#else
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, NULL, "command too long");
#endif
		return NULL;
	}
	strncat(cfg->cmd, arg, MAX_CMD-1);
	return NULL;
}

/* The actual limit handler */
#ifdef APACHE2
static int ipblock_handler(request_rec *r, int lookup_uri) {
#else
static int ipblock_handler(request_rec *r) {
#endif
	/* get configuration information */
	ipblock_config *cfg = (ipblock_config *) ap_get_module_config(r->server->module_config, &ipblock_module);

	/* A limit value of 0 or less, by convention, means no limit. */
	if (cfg->limit <= 0)
		return DECLINED;

	/* loop index variables */
	int i;
	int j;
	/* running count of number of connections from this address */
	int ip_count = 0;
#ifdef APACHE2
	/* scoreboard data structure */
	worker_score *ws_record;
#else
	/* scoreboard data structure */
    short_score score_record;
#endif
	/* We decline to handle subrequests: otherwise, in the next step we
	 * could get into an infinite loop. */
	if (!ap_is_initial_req(r)) {
#ifdef APACHE2
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "SKIPPED: Not initial request");
#else
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, r, "SKIPPED: Not initial request");
#endif
		return DECLINED;
	}

	/* Count up the number of connections we are handling right now from this IP address */
#ifdef APACHE2
	for (i = 0; i < server_limit; ++i) {
		for (j = 0; j < thread_limit; ++j) {
			ws_record = ap_get_scoreboard_worker(i, j);
			switch (ws_record->status) {
				case SERVER_BUSY_READ:
				case SERVER_BUSY_WRITE:
				case SERVER_BUSY_KEEPALIVE:
				case SERVER_BUSY_LOG:
				case SERVER_BUSY_DNS:
				case SERVER_CLOSING:
				case SERVER_GRACEFUL:
					if (strcmp(r->connection->remote_ip, ws_record->client) == 0)
						ip_count++;
					break;
				default:
					break;
			}
		}
	}
#else
	for (i = 0; i < HARD_SERVER_LIMIT; ++i) {
		score_record = ap_scoreboard_image->servers[i];
		switch (score_record.status) {
			case SERVER_BUSY_READ:
			case SERVER_BUSY_WRITE:
			case SERVER_BUSY_KEEPALIVE:
			case SERVER_BUSY_DNS:
			case SERVER_GRACEFUL:
				if ((strcmp(r->connection->remote_ip, score_record.client) == 0)
#ifdef RECORD_FORWARD
						|| (strcmp(ap_table_get(r->headers_in, "X-Forwarded-For"), score_record.fwdclient) == 0)
#endif
				) { 
					ip_count++;
				}   
				break;
			case SERVER_DEAD:
			case SERVER_READY:
			case SERVER_STARTING:
			case SERVER_BUSY_LOG:
				break;
		}
	}
#endif /* APACHE2 */

#ifdef APACHE2
	ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
#else
	ap_log_rerror(APLOG_MARK, APLOG_DEBUG, r,
#endif
		"vhost: %s  uri: %s  current: %d  limit: %d",
		r->server->server_hostname, r->uri, ip_count, cfg->limit);

	if (ip_count > cfg->limit) {
#ifdef APACHE2
		ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, "Rejected, too many connections from this host.");
#else
		ap_log_rerror(APLOG_MARK, APLOG_INFO, r, "Rejected, too many connections from this host.");
#endif
		/* set an environment variable */
#ifdef APACHE2
		apr_table_setn(r->subprocess_env, "BLOCKIP", "1");
#else
		ap_table_setn(r->subprocess_env, "BLOCKIP", "1");
#endif
		char *args[4];
		args[0] = cfg->cmd;
		args[1] = r->connection->remote_ip;
#ifdef APACHE2
		args[2] = apr_psprintf(r->pool, "%d %d %s%s", (int) ip_count, (int) cfg->limit, r->server->server_hostname, r->uri);
#else
		args[2] = ap_psprintf(r->pool, "%d %d %s%s", (int) ip_count, (int) cfg->limit, r->server->server_hostname, r->uri);
#endif
		args[3] = NULL;
		execve(cfg->cmd, args, NULL);
		/* return 503 */
		return HTTP_SERVICE_UNAVAILABLE;
	}

	return OK;
}

/* Array describing structure of configuration directives */
static const command_rec ipblock_cmds[] = {
#ifdef APACHE2
    AP_INIT_TAKE1("IPBlockLimit", ipblock_limit_cmd, NULL, RSRC_CONF,
     "maximum simultaneous connections per IP address after which the IP will be blocked"),
    AP_INIT_TAKE1("IPBlockCmd", ipblock_parse_cmd, NULL, RSRC_CONF,
     "the command that will be executed once an IP reaches the limit"),
    {NULL},
#else
    {"IPBlockLimit", ipblock_limit_cmd, NULL, RSRC_CONF, TAKE1,
     "maximum simultaneous connections per IP address after which the IP will be blocked"},
    {"IPBlockCmd", ipblock_parse_cmd, NULL, RSRC_CONF, TAKE1,
     "the command that will be executed once an IP reaches the limit"},
    {NULL},
#endif
};

#ifdef APACHE2
static void register_hooks(apr_pool_t *p) {
	ap_hook_quick_handler(ipblock_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA ipblock_module = {
	STANDARD20_MODULE_STUFF,
	NULL,			/* per-directory config creator */
	NULL,			/* dir config merger */
	ipblock_create_config,			/* server config creator */
	NULL,			/* server config merger */
	ipblock_cmds,	/* command table */
	register_hooks, /* set up other request processing hooks */
};
#else
module MODULE_VAR_EXPORT ipblock_module = {
	STANDARD_MODULE_STUFF,
	NULL,		/* initializer */
	NULL,		/* dir config creater */
	NULL,		/* dir merger --- default is to override */
	ipblock_create_config,		/* server config */
	NULL,		/* merge server config */
	ipblock_cmds,		/* command table */
	NULL,		/* handlers */
	NULL,		/* filename translation */
	NULL,		/* check_user_id */
	NULL,		/* check auth */
	ipblock_handler,		/* check access */
	NULL,		/* type_checker */
	NULL,		/* fixups */
	NULL,		/* logger */
	NULL,		/* header parser */
	NULL,		/* child_init */
	NULL,		/* child_exit */
	NULL 		/* post read-request */
};
#endif // APACHE2
