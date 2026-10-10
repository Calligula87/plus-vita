// SPDX-License-Identifier: AGPL-3.0-only

#include "socket_vita.h"

#include "network.h"

#include <errno.h>
#include <psp2/net/net.h>

/*
 * VitaSDK newlib keeps POSIX file descriptors in this public-linkage table.
 * Its socket() wrapper calls a second network initializer and gives the native
 * socket an empty name; the latter crashes in SceNet for our Takion UDP socket
 * on physical hardware.  Keep the ABI layout in sync with VitaSDK's
 * newlib/libc/sys/vita/vitadescriptor.h and register a named native socket so
 * the existing POSIX select/send/recv/close calls remain valid.
 */
typedef enum
{
	VITA_DESCRIPTOR_FILE,
	VITA_DESCRIPTOR_DIRECTORY,
	VITA_DESCRIPTOR_SOCKET,
	VITA_DESCRIPTOR_TTY,
	VITA_DESCRIPTOR_PIPE,
} PlusVitaDescriptorType;

typedef struct
{
	int sce_uid;
	PlusVitaDescriptorType type;
	int ref_count;
	char *filename;
	int flags;
} PlusVitaDescriptorTranslation;

extern PlusVitaDescriptorTranslation *__vita_fdmap[];
extern int __vita_acquire_descriptor(void);
extern int __vita_scenet_errno_to_errno(int error);

static int last_native_result;
static int last_net_errno;

int plus_vita_socket_last_native_result(void)
{
	return last_native_result;
}

int plus_vita_socket_last_net_errno(void)
{
	return last_net_errno;
}

int plus_vita_socket_open(const char *name, int domain, int type, int protocol)
{
	last_native_result = 0;
	last_net_errno = 0;

	if(!plus_vita_network_is_initialized())
	{
		errno = ENETDOWN;
		return -1;
	}

	if(!name || !name[0])
		name = "plus-vita";

	int native_socket = sceNetSocket(name, domain, type, protocol);
	if(native_socket < 0)
	{
		last_native_result = native_socket;
		int *net_errno = sceNetErrnoLoc();
		if(net_errno)
			last_net_errno = *net_errno;
		errno = __vita_scenet_errno_to_errno(native_socket);
		return -1;
	}

	int descriptor = __vita_acquire_descriptor();
	if(descriptor < 0)
	{
		sceNetSocketClose(native_socket);
		errno = EMFILE;
		return -1;
	}

	__vita_fdmap[descriptor]->sce_uid = native_socket;
	__vita_fdmap[descriptor]->type = VITA_DESCRIPTOR_SOCKET;
	return descriptor;
}
