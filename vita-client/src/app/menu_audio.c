// SPDX-License-Identifier: AGPL-3.0-only
#include "menu_audio.h"
#include <psp2/audioout.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MENU_AUDIO_BLOCK 512u
/* sceAudioOutOpenPort rejects lengths that are not multiples of 64. */
_Static_assert(MENU_AUDIO_BLOCK%64u==0,"Vita audio buffer granularity");
#define MENU_AUDIO_RATE 48000u
#define MENU_AUDIO_MAX_SAMPLES (MENU_AUDIO_RATE*32u)
struct plus_vita_menu_audio_t {
	int16_t *samples;
	unsigned count,volume;
	bool running,stop;
	pthread_t thread;
	pthread_mutex_t mutex;
};
static uint32_t little32(const unsigned char *bytes)
{ return (uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24); }

static void *menu_audio_worker(void *user)
{
	PlusVitaMenuAudio *audio=user;
	int port=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,MENU_AUDIO_BLOCK,MENU_AUDIO_RATE,SCE_AUDIO_OUT_MODE_MONO);
	if(port<0) return NULL;
	int volume[2]={SCE_AUDIO_OUT_MAX_VOL,SCE_AUDIO_OUT_MAX_VOL};
	if(sceAudioOutSetVolume(port,SCE_AUDIO_VOLUME_FLAG_L_CH|SCE_AUDIO_VOLUME_FLAG_R_CH,volume)<0) {
		sceAudioOutReleasePort(port);return NULL;
	}
	unsigned position=0;
	int gain=0;
	int16_t block[MENU_AUDIO_BLOCK];
	for(;;) {
		pthread_mutex_lock(&audio->mutex);
		bool stop=audio->stop;
		int target=stop ? 0 : (int)audio->volume*256;
		pthread_mutex_unlock(&audio->mutex);
		int initial=gain;
		/* Gain changes take about 100ms; shutdown fades within its final 11ms
		 * output block, without a separate sleep before stream entry. */
		int next=stop ? 0 : gain+(target-gain)/10;
		if(!stop && next==gain && target!=gain) next+=target>gain ? 1:-1;
		for(unsigned i=0;i<MENU_AUDIO_BLOCK;i++) {
			int sample_gain=initial+(next-initial)*(int)(i+1)/(int)MENU_AUDIO_BLOCK;
			block[i]=(int16_t)((int32_t)audio->samples[position]*sample_gain/25600);
			if(++position==audio->count) position=0;
		}
		gain=next;
		if(sceAudioOutOutput(port,block)<0 || stop) break;
	}
	sceAudioOutOutput(port,NULL);
	sceAudioOutReleasePort(port);
	return NULL;
}

PlusVitaMenuAudio *plus_vita_menu_audio_create(const char *path)
{
	FILE *file=path ? fopen(path,"rb") : NULL;
	if(!file) return NULL;
	unsigned char header[16];
	bool valid=fread(header,1,sizeof(header),file)==sizeof(header)
		&& !memcmp(header,"PVAUDIO1",8) && little32(header+8)==MENU_AUDIO_RATE;
	unsigned count=valid ? little32(header+12):0;
	if(count<MENU_AUDIO_BLOCK || count>MENU_AUDIO_MAX_SAMPLES) valid=false;
	PlusVitaMenuAudio *audio=valid ? calloc(1,sizeof(*audio)):NULL;
	if(audio) { audio->samples=malloc((size_t)count*2);valid=audio->samples!=NULL; }
	else valid=false;
	if(valid) valid=fread(audio->samples,2,count,file)==count && fgetc(file)==EOF && !ferror(file);
	if(fclose(file)) valid=false;
	if(!valid) { if(audio) { free(audio->samples);free(audio); }return NULL; }
	unsigned char *bytes=(unsigned char *)audio->samples;
	for(unsigned i=0;i<count;i++) audio->samples[i]=(int16_t)((unsigned)bytes[i*2]|((unsigned)bytes[i*2+1]<<8));
	audio->count=count;
	if(pthread_mutex_init(&audio->mutex,NULL)) { free(audio->samples);free(audio);return NULL; }
	return audio;
}

void plus_vita_menu_audio_update(PlusVitaMenuAudio *audio,PlusVitaPresentation preferences,bool foreground)
{
	if(!audio) return;
	bool audible=foreground && !preferences.muted && preferences.volume>0 && preferences.volume<=100;
	pthread_mutex_lock(&audio->mutex);
	audio->volume=preferences.volume<=100 ? preferences.volume:0;
	if(!audible) audio->stop=true;
	pthread_mutex_unlock(&audio->mutex);
	if(audio->running && !audible) {
		pthread_join(audio->thread,NULL);audio->running=false;
	}
	if(!audio->running && audible) {
		pthread_mutex_lock(&audio->mutex);audio->stop=false;pthread_mutex_unlock(&audio->mutex);
		pthread_attr_t attributes;
		if(pthread_attr_init(&attributes)) return;
		if(!pthread_attr_setstacksize(&attributes,64*1024)) audio->running=pthread_create(&audio->thread,&attributes,menu_audio_worker,audio)==0;
		pthread_attr_destroy(&attributes);
	}
}

void plus_vita_menu_audio_close(PlusVitaMenuAudio *audio)
{
	if(!audio) return;
	plus_vita_menu_audio_update(audio,(PlusVitaPresentation){0},false);
	pthread_mutex_destroy(&audio->mutex);free(audio->samples);free(audio);
}
