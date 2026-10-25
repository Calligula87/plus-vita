# Explicit anonymous release inputs. Never echo a rejected value into CI output.
if(PLUS_VITA_RELEASE_CANDIDATE)
  foreach(rejected PLUS_VITA_DIAGNOSTIC_APP PLUS_VITA_FONT_COMPARISON
      PLUS_VITA_TLS_PROBE_URL PLUS_VITA_TLS_TRUSTED_CA_FILE
      PLUS_VITA_PUBLIC_HTTPS_URL PLUS_VITA_POST_ECHO_URL PLUS_VITA_PUBLIC_APOLLO_TEST)
    if(${rejected})
      message(FATAL_ERROR "Anonymous candidate forbids test interfaces, URLs and local certificates")
    endif()
  endforeach()
  if(NOT PLUS_VITA_INPUT_WORKER OR NOT PLUS_VITA_DEFERRED_LOG OR NOT PLUS_VITA_FONT_DEFAULT_FREETYPE)
    message(FATAL_ERROR "Anonymous candidate requires the accepted input, logging and font profile")
  endif()
endif()
