/* hooks.c -- Hooks triggered by events
   This file is part of Redshift.

   Redshift is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Redshift is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Redshift.  If not, see <http://www.gnu.org/licenses/>.

   Copyright (c) 2014  Jon Lund Steffensen <jonlst@gmail.com>
*/

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#ifndef _WIN32
# include <pwd.h>
#endif

#include "hooks.h"
#include "redshift.h"

#define MAX_HOOK_PATH  4096


/* Names of periods supplied to scripts. */
static const char *period_names[] = {
	"none",
	"daytime",
	"night",
	"transition"
};


static DIR *
try_open_hooks(const char *base_dir, const char *app_name, char *hp)
{
	snprintf(hp, MAX_HOOK_PATH, "%s/%s/hooks", base_dir, app_name);
	return opendir(hp);
}

/* Try to open the directory containing hooks. HP is a string
   of MAX_HOOK_PATH length that will be filled with the path
   of the returned directory. */
static DIR *
open_hooks_dir(char *hp)
{
	const char *apps[] = { "jarheart", "redshift", NULL };
	char *env = getenv("XDG_CONFIG_HOME");
	if (env != NULL && env[0] != '\0') {
		for (int i = 0; apps[i] != NULL; i++) {
			DIR *d = try_open_hooks(env, apps[i], hp);
			if (d != NULL) return d;
		}
	}

	env = getenv("HOME");
	if (env != NULL && env[0] != '\0') {
		char base[MAX_HOOK_PATH];
		snprintf(base, sizeof(base), "%s/.config", env);
		for (int i = 0; apps[i] != NULL; i++) {
			DIR *d = try_open_hooks(base, apps[i], hp);
			if (d != NULL) return d;
		}
	}

#ifndef _WIN32
	struct passwd *pwd = getpwuid(getuid());
	if (pwd != NULL) {
		char base[MAX_HOOK_PATH];
		snprintf(base, sizeof(base), "%s/.config", pwd->pw_dir);
		for (int i = 0; apps[i] != NULL; i++) {
			DIR *d = try_open_hooks(base, apps[i], hp);
			if (d != NULL) return d;
		}
	}
#endif

	return NULL;
}

/* Run hooks with a signal that the period changed. */
void
hooks_signal_period_change(period_t prev_period, period_t period)
{
	char hooksdir_path[MAX_HOOK_PATH];
	DIR *hooks_dir = open_hooks_dir(hooksdir_path);
	if (hooks_dir == NULL) return;

	struct dirent* ent;
	while ((ent = readdir(hooks_dir)) != NULL) {
		/* Skip hidden and special files (., ..) */
		if (ent->d_name[0] == '\0' || ent->d_name[0] == '.') continue;

		char *hook_name = ent->d_name;
		char hook_path[MAX_HOOK_PATH + 256 + 1];
		snprintf(hook_path, sizeof(hook_path), "%s/%s",
			 hooksdir_path, hook_name);

#ifndef _WIN32
		/* Fork and exec the hook. We close stdout
		   so the hook cannot interfere with the normal
		   output. */
		pid_t pid = fork();
		if (pid == (pid_t)-1) {
			perror("fork");
			continue;
		} else if (pid == 0) { /* Child */
			close(STDOUT_FILENO);

			int r = execl(hook_path, hook_name,
				      "period-changed",
				      period_names[prev_period],
				      period_names[period], NULL);
			if (r < 0 && errno != EACCES) perror("execl");

			/* Only reached on error */
			_exit(EXIT_FAILURE);
		}
#endif
	}
	closedir(hooks_dir);
}

/* Run hooks with a signal that the status changed (enabled, disabled, paused). */
void
hooks_signal_status_change(const char *event_name, const char *status_name)
{
	char hooksdir_path[MAX_HOOK_PATH];
	DIR *hooks_dir = open_hooks_dir(hooksdir_path);
	if (hooks_dir == NULL) return;

	struct dirent* ent;
	while ((ent = readdir(hooks_dir)) != NULL) {
		if (ent->d_name[0] == '\0' || ent->d_name[0] == '.') continue;

		char *hook_name = ent->d_name;
		char hook_path[MAX_HOOK_PATH + 256 + 1];
		snprintf(hook_path, sizeof(hook_path), "%s/%s",
			 hooksdir_path, hook_name);

#ifndef _WIN32
		pid_t pid = fork();
		if (pid == (pid_t)-1) {
			perror("fork");
			continue;
		} else if (pid == 0) { /* Child */
			close(STDOUT_FILENO);

			int r = execl(hook_path, hook_name,
				      event_name, status_name, NULL);
			if (r < 0 && errno != EACCES) perror("execl");

			_exit(EXIT_FAILURE);
		}
#endif
	}
	closedir(hooks_dir);
}
