/* ipc.h -- Inter-process communication header for Jarheart
   This file is part of Jarheart.

   Jarheart is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Jarheart is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Jarheart.  If not, see <http://www.gnu.org/licenses/>.

   Copyright (c) 2026  Jarheart Contributors
*/

#ifndef JARHEART_IPC_H
#define JARHEART_IPC_H

#include <stddef.h>
#include <time.h>
#include "redshift.h"

#define JARHEART_IPC_BUF_SIZE 2048

typedef struct {
	int listen_fd;
	char socket_path[108];
	char symlink_path[108];
} ipc_t;

typedef struct {
	int disabled;
	time_t pause_until;
	int override_temp;
	period_t period;
	double transition_prog;
	color_setting_t current_setting;
	location_t location;
	const char *method_name;
	int requested_exit;
	int state_changed;
} daemon_ipc_state_t;

/* Parse human duration string like "30m", "1h", "45s", "1800" into seconds.
   Returns >= 0 on success, -1 on error. */
int ipc_parse_duration(const char *str);

/* Format a response for a given command against the daemon state.
   Returns 0 on success. */
int ipc_dispatch_command(
	const char *cmd_line, daemon_ipc_state_t *state,
	char *response_buf, size_t response_buf_size);

#ifndef _WIN32

/* Initialize daemon IPC server socket.
   Returns 0 on success, -EEXIST if another daemon is running, or -1 on error. */
int ipc_init(ipc_t *ipc);

/* Get the file descriptor to poll on. */
int ipc_get_fd(const ipc_t *ipc);

/* Handle an incoming connection on listen_fd. */
int ipc_handle_connection(ipc_t *ipc, daemon_ipc_state_t *state);

/* Clean up and unlink socket files. */
void ipc_cleanup(ipc_t *ipc);

/* Send command from client to running daemon and receive response.
   Returns 0 on success, < 0 on connection failure. */
int ipc_client_send_command(
	const char *cmd, char *response_buf, size_t response_buf_size);

/* Check if argv[1] is an IPC subcommand and if so execute it.
   Returns exit code (0 or 1) if handled, or -1 if not a subcommand. */
int ipc_client_dispatch(int argc, char *argv[]);

#else /* _WIN32 stubs */

static inline int ipc_init(ipc_t *ipc) { (void)ipc; return -1; }
static inline int ipc_get_fd(const ipc_t *ipc) { (void)ipc; return -1; }
static inline int ipc_handle_connection(ipc_t *ipc, daemon_ipc_state_t *s) { (void)ipc; (void)s; return -1; }
static inline void ipc_cleanup(ipc_t *ipc) { (void)ipc; }
static inline int ipc_client_send_command(const char *c, char *r, size_t s) { (void)c; (void)r; (void)s; return -1; }
static inline int ipc_client_dispatch(int argc, char *argv[]) { (void)argc; (void)argv; return -1; }

#endif /* _WIN32 */

#endif /* JARHEART_IPC_H */
