# Preserve a confirmed credential rejection independently of network/service
# failures. Upstream collapses every non-2xx into AUTHORIZATION_FAILED.
if(NOT DEFINED PLUS_VITA_CLOUD_SESSION_SOURCE)
  set(PLUS_VITA_CLOUD_SESSION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/cloudsession_vita.c")
endif()
file(READ "${PYLUX_ROOT}/lib/src/cloudsession.c" PLUS_VITA_CLOUD_SESSION_TEXT)
set(PLUS_VITA_AUTH_OLD [=[
	CHIAKI_LOGE(log, "[CLOUDSESSION] authorizeCheck failed (HTTP %ld); NPSSO likely expired", status);
	return CHIAKI_ERR_UNKNOWN;
]=])
set(PLUS_VITA_AUTH_NEW [=[
	return status == 401 ? CHIAKI_ERR_HTTP_NONOK : CHIAKI_ERR_UNKNOWN;
]=])
string(FIND "${PLUS_VITA_CLOUD_SESSION_TEXT}" "${PLUS_VITA_AUTH_OLD}" PLUS_VITA_AUTH_POS)
if(PLUS_VITA_AUTH_POS EQUAL -1)
  message(FATAL_ERROR "Pylux authorization status patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_AUTH_OLD}" "${PLUS_VITA_AUTH_NEW}" PLUS_VITA_CLOUD_SESSION_TEXT "${PLUS_VITA_CLOUD_SESSION_TEXT}")
set(PLUS_VITA_AUTH_RESULT_OLD "out->err = auth_err == CHIAKI_ERR_NETWORK ? CHIAKI_ERR_NETWORK : CHIAKI_ERR_UNKNOWN;")
string(FIND "${PLUS_VITA_CLOUD_SESSION_TEXT}" "${PLUS_VITA_AUTH_RESULT_OLD}" PLUS_VITA_AUTH_POS)
if(PLUS_VITA_AUTH_POS EQUAL -1)
  message(FATAL_ERROR "Pylux authorization result patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_AUTH_RESULT_OLD}" "out->err = auth_err;" PLUS_VITA_CLOUD_SESSION_TEXT "${PLUS_VITA_CLOUD_SESSION_TEXT}")
file(WRITE "${PLUS_VITA_CLOUD_SESSION_SOURCE}" "${PLUS_VITA_CLOUD_SESSION_TEXT}")
