#pragma once
#define SCE_AUDIO_OUT_PORT_TYPE_MAIN 0
#define SCE_AUDIO_OUT_MODE_STEREO 1
#define SCE_AUDIO_OUT_MODE_MONO 0
#define SCE_AUDIO_OUT_MAX_VOL 32768
#define SCE_AUDIO_VOLUME_FLAG_L_CH 1
#define SCE_AUDIO_VOLUME_FLAG_R_CH 2
#define SCE_AUDIO_OUT_CONFIG_TYPE_LEN 0
#define SCE_AUDIO_OUT_CONFIG_TYPE_FREQ 1
#define SCE_AUDIO_OUT_CONFIG_TYPE_MODE 2
int sceAudioOutOpenPort(int, int, int, int);
int sceAudioOutSetVolume(int, int, const int *);
int sceAudioOutGetConfig(int, int);
int sceAudioOutOutput(int, const void *);
int sceAudioOutReleasePort(int);
