/* SPDX-License-Identifier: LGPL-2.1+ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "conf.h"
#include "config.h"
#include "log.h"
#include "start.h"
#include "raw_syscalls.h"
#include "utils.h"

lxc_log_define(execute, start);

static int execute_start(struct lxc_handler *handler, void* data)
{
	int argc_add, j;
	char **argv;
	int argc = 0, i = 0;
	struct execute_args *my_args = data;

	while (my_args->argv[argc++]);

	/* lxc-init -n name -- [argc] NULL -> 5 */
	argc_add = 5;
	if (my_args->quiet)
		argc_add++;

	if (!handler->conf->rootfs.path)
		argc_add += 2;

	argv = malloc((argc + argc_add) * sizeof(*argv));
	if (!argv) {
		SYSERROR("Allocating init args failed");
		goto out1;
	}

	if (my_args->init_path)
		argv[i++] = my_args->init_path;
	else
		argv[i++] = "lxc-init";

	argv[i++] = "-n";
	argv[i++] = (char *)handler->name;

	if (my_args->quiet)
		argv[i++] = "--quiet";

	if (!handler->conf->rootfs.path) {
		argv[i++] = "-P";
		argv[i++] = (char *)handler->lxcpath;
	}

	argv[i++] = "--";
	for (j = 0; j < argc; j++)
		argv[i++] = my_args->argv[j];
	argv[i++] = NULL;

	NOTICE("Exec'ing \"%s\"", my_args->argv[0]);

	if (my_args->init_fd >= 0)
		lxc_raw_execveat(my_args->init_fd, "", argv, environ, AT_EMPTY_PATH);
	else
		execvp(argv[0], argv);
	SYSERROR("Failed to exec %s", argv[0]);

	free(argv);
out1:
	return 1;
}

static int execute_post_start(struct lxc_handler *handler, void* data)
{
	struct execute_args *my_args = data;
	NOTICE("'%s' started with pid '%d'", my_args->argv[0], handler->pid);
	return 0;
}

static struct lxc_operations execute_start_ops = {
	.start = execute_start,
	.post_start = execute_post_start
};

int lxc_execute(const char *name, char *const argv[], int quiet,
		struct lxc_handler *handler, const char *lxcpath,
		bool daemonize, int *error_num)
{
	struct execute_args args = {.argv = argv, .quiet = quiet};

	TRACE("Doing lxc_execute");
	handler->conf->is_execute = true;
	return __lxc_start(name, handler, &execute_start_ops, &args, lxcpath,
			   daemonize, error_num);
}
