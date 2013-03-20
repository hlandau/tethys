/* ircd-micro, main.c -- entry point
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

struct u_io base_io;

short opt_port = 6667;

usage(argv0, code)
{
	printf("Usage: %s [OPTIONS]\n", argv0);
	printf("Options:\n");
	printf("  -h         Print this help and exit\n");
	printf("  -p PORT    The port to listen on for connections\n");
	exit(code);
}

#define INIT(fn) if ((err = (fn)()) < 0) return err
#define COMMAND_DEF(cmds) extern struct u_cmd cmds[]
#define COMMAND(cmds) if ((err = u_cmds_reg(cmds)) < 0) return err
COMMAND_DEF(c_reg);
COMMAND_DEF(c_user);
int init()
{
	int err;
	FILE *f;

	signal(SIGPIPE, SIG_IGN);

	INIT(init_util);
	INIT(init_dns);
	INIT(init_conf);
	INIT(init_user);
	INIT(init_cmd);
	INIT(init_server);
	COMMAND(c_reg);
	COMMAND(c_user);

	u_io_init(&base_io);
	u_dns_use_io(&base_io);

	f = fopen("etc/micro.conf", "r");
	if (f == NULL) {
		u_log(LG_SEVERE, "Could not find etc/micro.conf!");
		return -1;
	}
	u_conf_read(f);
	fclose(f);

	return 0;
}

void test_cb(status, res, priv)
int status;
char *res;
void *priv;
{
	char *error = NULL;
	switch (status) {
	case DNS_OKAY:
		u_log(LG_DEBUG, "DNS okay: %s", res);
		break;
	case DNS_NXDOMAIN:
		error = "NXDOMAIN";
		break;
	case DNS_TIMEOUT:
		error = "TIMEOUT";
		break;
	case DNS_INVALID:
		error = "INVALID";
		break;
	case DNS_TOOLONG:
		error = "TOOLONG";
		break;
	case DNS_OTHER:
		error = "OTHER";
		break;
	}
	if (error != NULL)
		u_log(LG_DEBUG, "DNS error: %s", error);
}

extern char *optarg;

int main(argc, argv)
int argc;
char *argv[];
{
	int c;

	u_log(LG_INFO, "%s starting...", PACKAGE_FULLNAME);

	while ((c = getopt(argc, argv, "hp:")) != -1) {
		switch(c) {
		case 'h':
			usage(argv[0], 0);
			break;
		case 'p':
			opt_port = atoi(optarg);
			break;
		case '?':
			usage(argv[0], 1);
			break;
		}
	}

	if (init() < 0) {
		u_log(LG_ERROR, "Initialization failed");
		return 1;
	}

	u_rdns("129.219.10.241", test_cb, NULL);

	if (!u_conn_origin_create(&base_io, INADDR_ANY, opt_port)) {
		u_log(LG_SEVERE, "Could not create connection origin. Bailing");
		return 1;
	}

	u_log(LG_INFO, "Entering IO loop");

	u_io_poll(&base_io);

	u_log(LG_VERBOSE, "IO loop died. Bye bye!");

	return 0;
}
