// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_SOCKET_VITA_H
#define PLUS_VITA_SOCKET_VITA_H

int plus_vita_socket_open(const char *name, int domain, int type, int protocol);
int plus_vita_socket_last_native_result(void);
int plus_vita_socket_last_net_errno(void);

#endif
