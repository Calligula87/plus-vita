// SPDX-License-Identifier: AGPL-3.0-only
#include "result.h"

#include <chiaki/cloudsession.h>
#include <chiaki/session.h>
#include <chiaki/time.h>

#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

struct probe_session {
	ChiakiSession session;
	struct probe_video video;
	int32_t previous_profile_frame;
	atomic_bool video_ready;
	atomic_int quit_reason;
};

_Static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "The signal handler requires a lock-free flag");
static atomic_bool interrupted;

static void interrupt_probe(int signal_number)
{
	(void)signal_number;
	atomic_store(&interrupted, true);
}

static bool cancelled(void *user)
{
	(void)user;
	return atomic_load(&interrupted);
}

static void session_event(ChiakiEvent *event, void *user)
{
	struct probe_session *probe = user;
	if(event->type == CHIAKI_EVENT_QUIT)
		atomic_store(&probe->quit_reason, event->quit.reason);
}

static bool video_sample(uint8_t *data, size_t size, int32_t lost, bool recovered, void *user)
{
	struct probe_session *probe = user;
	if(atomic_load(&probe->video_ready)) return true;
	/* Pylux calls back on the receive thread, first with the active StreamInfo
	 * header, then with assembled video. A profile change can flush one old frame. */
	const ChiakiVideoReceiver *receiver = probe->session.stream_connection.video_receiver;
	if(!receiver || receiver->profile_cur < 0 || (size_t)receiver->profile_cur >= receiver->profiles_count)
		return true;
	const ChiakiVideoProfile *profile = &receiver->profiles[receiver->profile_cur];
	if(data == profile->header)
	{
		probe->previous_profile_frame = receiver->frame_index_cur;
		probe_video_header(&probe->video, profile->width, profile->height, data, size);
	}
	else if(receiver->frame_index_cur != probe->previous_profile_frame)
	{
		if(probe_video_sample(&probe->video, data, size, lost != 0 || recovered))
			atomic_store(&probe->video_ready, true);
	}
	return true;
}

static enum probe_status inspect_session(struct probe_session *probe,
	ChiakiCloudProvisionResult *allocation, ChiakiLog *log, int *code, FILE *output)
{
	ChiakiConnectInfo connect = {
		.ps5 = true,
		.host = allocation->server_ip,
		.video_profile = { .width = PROBE_DEFAULT_WIDTH, .height = PROBE_DEFAULT_HEIGHT,
			.max_fps = PROBE_DEFAULT_FPS, .bitrate = PROBE_DEFAULT_BITRATE_KBPS,
			.codec = CHIAKI_CODEC_H265 },
		.packet_loss_max = 0.05,
		.service_type = CHIAKI_SERVICE_TYPE_PSCLOUD,
		.cloud_launch_spec = allocation->launch_spec,
		.cloud_handshake_key = allocation->handshake_key,
		.cloud_session_id = allocation->session_id,
		.cloud_port = (uint16_t)allocation->server_port,
		.cloud_psn_wrapper_type = allocation->psn_wrapper_type,
		.cloud_mtu_in = allocation->mtu_in,
		.cloud_mtu_out = allocation->mtu_out,
		.cloud_rtt_us = allocation->rtt_us,
	};
	*code = chiaki_session_init(&probe->session, &connect, log);
	if(*code != CHIAKI_ERR_SUCCESS) return PROBE_SESSION_INIT;
	chiaki_session_set_event_cb(&probe->session, session_event, probe);
	chiaki_session_set_video_sample_cb(&probe->session, video_sample, probe);
	*code = chiaki_session_start(&probe->session);
	if(*code != CHIAKI_ERR_SUCCESS)
	{
		chiaki_session_fini(&probe->session);
		return PROBE_SESSION_START;
	}

	const uint64_t deadline = chiaki_time_now_monotonic_ms() + 45000;
	while(!atomic_load(&interrupted) && !atomic_load(&probe->video_ready)
		&& atomic_load(&probe->quit_reason) == CHIAKI_QUIT_REASON_NONE
		&& chiaki_time_now_monotonic_ms() < deadline)
		nanosleep(&(struct timespec){ .tv_nsec = 50 * 1000 * 1000 }, NULL);

	enum probe_status status = PROBE_VIDEO_TIMEOUT;
	*code = atomic_load(&probe->quit_reason);
	if(atomic_load(&interrupted)) status = PROBE_INTERRUPTED;
	else if(atomic_load(&probe->video_ready)) status = PROBE_COMPLETE;
	else if(*code != CHIAKI_QUIT_REASON_NONE) status = PROBE_DISCONNECTED;
	if(status != PROBE_DISCONNECTED) *code = 0;

	ChiakiErrorCode stop = chiaki_session_stop(&probe->session);
	ChiakiErrorCode join = chiaki_session_join(&probe->session);
	if(join != CHIAKI_ERR_SUCCESS)
		exit(probe_report(output, &(struct probe_video){ 0 }, PROBE_CLEANUP, join));
	chiaki_session_fini(&probe->session);
	if(stop != CHIAKI_ERR_SUCCESS)
	{
		*code = stop;
		return PROBE_CLEANUP;
	}
	return status;
}

static bool valid_value(const char *value, size_t max, const char *alphabet)
{
	size_t size = strlen(value);
	return size > 0 && size <= max && strspn(value, alphabet) == size;
}

/* Keep a report-only descriptor and discard direct writes by dependencies. */
static FILE *isolate_diagnostics(void)
{
	const struct rlimit no_core = { 0, 0 };
	if(setrlimit(RLIMIT_CORE, &no_core) < 0 || unsetenv("SSLKEYLOGFILE") < 0) return NULL;
	int report_fd = fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 3);
	if(report_fd < 0) return NULL;
	FILE *output = fdopen(report_fd, "w");
	if(!output) { close(report_fd); return NULL; }
	int sink = open("/dev/null", O_WRONLY);
	if(sink < 0 || dup2(sink, STDERR_FILENO) < 0 || dup2(sink, STDOUT_FILENO) < 0)
	{
		if(sink >= 0) close(sink);
		fclose(output);
		return NULL;
	}
	if(sink != STDERR_FILENO) close(sink);
	return output;
}

int main(int argc, char **argv)
{
	struct probe_session probe = { 0 };
	const char identifier_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
	if(argc != 5 || !valid_value(argv[1], 127, identifier_chars)
		|| strlen(argv[2]) != 2 || !valid_value(argv[2], 2, "ABCDEFGHIJKLMNOPQRSTUVWXYZ")
		|| !valid_value(argv[3], 12, identifier_chars) || !valid_value(argv[4], 12, identifier_chars)
		|| isatty(STDIN_FILENO))
		return probe_report(stdout, &probe.video, PROBE_INPUT, 0);

	FILE *output = isolate_diagnostics();
	if(!output) return probe_report(stdout, &probe.video, PROBE_ISOLATION, 0);
	const struct sigaction action = { .sa_handler = interrupt_probe };
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	char npsso[258];
	if(!fgets(npsso, sizeof(npsso), stdin) || !strchr(npsso, '\n'))
		return probe_report(output, &probe.video, PROBE_INPUT, 0);
	npsso[strcspn(npsso, "\n")] = '\0';
	if(!valid_value(npsso, 256, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~+/="))
		return probe_report(output, &probe.video, PROBE_INPUT, 0);

	int code = chiaki_lib_init();
	if(code != CHIAKI_ERR_SUCCESS) return probe_report(output, &probe.video, PROBE_LIBRARY_INIT, code);
	ChiakiLog log;
	chiaki_log_init(&log, 0, NULL, NULL);
	const ChiakiCloudProvisionConfig config = {
		.service_type = "pscloud", .game_identifier = argv[1], .npsso = npsso,
		.store_country = argv[2], .store_lang = argv[3], .game_language = argv[4],
		.resolution = PROBE_DEFAULT_HEIGHT, .bitrate_kbps = PROBE_DEFAULT_BITRATE_KBPS,
		.is_cancelled = cancelled,
	};
	ChiakiCloudProvisionResult allocation;
	code = chiaki_cloud_provision_session(&config, &allocation, &log);
	enum probe_status status;
	if(atomic_load(&interrupted)) status = PROBE_INTERRUPTED;
	else if(code != CHIAKI_ERR_SUCCESS)
		status = probe_provisioning_failure(allocation.error_message);
	else if(strcmp(allocation.platform, "ps5") != 0 || !*allocation.server_ip
		|| allocation.server_port <= 0 || allocation.server_port > UINT16_MAX
		|| !allocation.handshake_key || !*allocation.handshake_key
		|| !allocation.launch_spec || !*allocation.launch_spec
		|| !allocation.session_id || !*allocation.session_id)
		status = PROBE_ALLOCATION;
	else
		status = inspect_session(&probe, &allocation, &log, &code, output);
	chiaki_cloud_provision_result_fini(&allocation);
	return probe_report(output, &probe.video, status, code);
}
