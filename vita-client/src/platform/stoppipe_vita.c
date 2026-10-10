// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki/stoppipe.h>
#include <chiaki/sock.h>
#include <chiaki/time.h>

#include <errno.h>
#include <psp2/kernel/threadmgr.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/socket.h>

/*
 * Vita newlib exposes pipe(), but the kernel implementation returns failure.
 * A loopback UDP pair is not a safe substitute here either: creating the
 * auxiliary socket can fault inside SceNet on retail Vita firmware.
 *
 * Preserve ChiakiStopPipe's public ABI and use fds[0] as an atomic cancellation
 * flag. Socket waits are split into short select() intervals so cancellation is
 * still observed promptly without allocating any extra kernel object.
 */

#define VITA_STOP_PIPE_MAGIC 0x56535450
#define VITA_STOP_PIPE_POLL_MS 20U

static bool stop_pipe_valid(const ChiakiStopPipe *stop_pipe)
{
	return stop_pipe
		&& __atomic_load_n(&stop_pipe->fds[1], __ATOMIC_ACQUIRE)
			== VITA_STOP_PIPE_MAGIC;
}

static bool stop_pipe_stopped(const ChiakiStopPipe *stop_pipe)
{
	return __atomic_load_n(&stop_pipe->fds[0], __ATOMIC_ACQUIRE) != 0;
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_stop_pipe_init(ChiakiStopPipe *stop_pipe)
{
	if(!stop_pipe) return CHIAKI_ERR_INVALID_DATA;
	__atomic_store_n(&stop_pipe->fds[0], 0, __ATOMIC_RELAXED);
	__atomic_store_n(&stop_pipe->fds[1], VITA_STOP_PIPE_MAGIC,
		__ATOMIC_RELEASE);
	return CHIAKI_ERR_SUCCESS;
}

CHIAKI_EXPORT void chiaki_stop_pipe_fini(ChiakiStopPipe *stop_pipe)
{
	if(!stop_pipe) return;
	__atomic_store_n(&stop_pipe->fds[0], 1, __ATOMIC_RELEASE);
	__atomic_store_n(&stop_pipe->fds[1], 0, __ATOMIC_RELEASE);
}

CHIAKI_EXPORT void chiaki_stop_pipe_stop(ChiakiStopPipe *stop_pipe)
{
	if(!stop_pipe_valid(stop_pipe)) return;
	__atomic_store_n(&stop_pipe->fds[0], 1, __ATOMIC_RELEASE);
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_stop_pipe_select_single(
	ChiakiStopPipe *stop_pipe, chiaki_socket_t fd, bool write,
	uint64_t timeout_ms)
{
	if(!stop_pipe_valid(stop_pipe)) return CHIAKI_ERR_INVALID_DATA;

	const bool finite_timeout = timeout_ms != UINT64_MAX;
	const uint64_t started_ms = chiaki_time_now_monotonic_ms();
	uint64_t deadline_ms = UINT64_MAX;
	if(finite_timeout && UINT64_MAX - started_ms >= timeout_ms)
		deadline_ms = started_ms + timeout_ms;

	for(;;)
	{
		if(stop_pipe_stopped(stop_pipe)) return CHIAKI_ERR_CANCELED;

		uint64_t wait_ms = VITA_STOP_PIPE_POLL_MS;
		if(finite_timeout)
		{
			uint64_t now_ms = chiaki_time_now_monotonic_ms();
			if(now_ms >= deadline_ms)
				wait_ms = 0;
			else if(deadline_ms - now_ms < wait_ms)
				wait_ms = deadline_ms - now_ms;
		}

		if(CHIAKI_SOCKET_IS_INVALID(fd))
		{
			if(wait_ms == 0) return CHIAKI_ERR_TIMEOUT;
			sceKernelDelayThread((unsigned int)(wait_ms * 1000));
			continue;
		}

		fd_set read_fds;
		fd_set write_fds;
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		FD_SET(fd, write ? &write_fds : &read_fds);

		struct timeval timeout_value;
		timeout_value.tv_sec = (long)(wait_ms / 1000);
		timeout_value.tv_usec = (long)((wait_ms % 1000) * 1000);

		int result = select(fd + 1, write ? NULL : &read_fds,
			write ? &write_fds : NULL, NULL, &timeout_value);
		if(result < 0)
		{
			if(errno == EINTR) continue;
			return CHIAKI_ERR_UNKNOWN;
		}

		if(stop_pipe_stopped(stop_pipe)) return CHIAKI_ERR_CANCELED;
		if(result > 0 && FD_ISSET(fd, write ? &write_fds : &read_fds))
			return CHIAKI_ERR_SUCCESS;

		if(finite_timeout
			&& chiaki_time_now_monotonic_ms() >= deadline_ms)
			return CHIAKI_ERR_TIMEOUT;
	}
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_stop_pipe_connect(ChiakiStopPipe *stop_pipe,
	chiaki_socket_t fd, struct sockaddr *address, size_t address_length,
	uint64_t timeout_ms)
{
	int result = connect(fd, address, (socklen_t)address_length);
	if(result >= 0) return CHIAKI_ERR_SUCCESS;

	if(CHIAKI_SOCKET_EINPROGRESS)
	{
		ChiakiErrorCode err = chiaki_stop_pipe_select_single(stop_pipe, fd,
			true, timeout_ms);
		if(err != CHIAKI_ERR_SUCCESS) return err;
	}
	else
	{
		return errno == ECONNREFUSED
			? CHIAKI_ERR_CONNECTION_REFUSED : CHIAKI_ERR_NETWORK;
	}

	struct sockaddr_storage peer;
	socklen_t peer_length = sizeof(peer);
	if(getpeername(fd, (struct sockaddr *)&peer, &peer_length) == 0)
		return CHIAKI_ERR_SUCCESS;
	if(errno != ENOTCONN) return CHIAKI_ERR_UNKNOWN;

	int socket_error = 0;
	socklen_t socket_error_length = sizeof(socket_error);
	if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error,
		&socket_error_length) < 0)
		return CHIAKI_ERR_UNKNOWN;
	switch(socket_error)
	{
		case 0: return CHIAKI_ERR_SUCCESS;
		case ETIMEDOUT: return CHIAKI_ERR_TIMEOUT;
		case ECONNREFUSED: return CHIAKI_ERR_CONNECTION_REFUSED;
		case EHOSTDOWN: return CHIAKI_ERR_HOST_DOWN;
		case EHOSTUNREACH: return CHIAKI_ERR_HOST_UNREACH;
		default: return CHIAKI_ERR_UNKNOWN;
	}
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_stop_pipe_reset(ChiakiStopPipe *stop_pipe)
{
	if(!stop_pipe_valid(stop_pipe)) return CHIAKI_ERR_INVALID_DATA;
	__atomic_store_n(&stop_pipe->fds[0], 0, __ATOMIC_RELEASE);
	return CHIAKI_ERR_SUCCESS;
}
