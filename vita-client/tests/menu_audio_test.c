// SPDX-License-Identifier: AGPL-3.0-only
#define _POSIX_C_SOURCE 200809L
#include "menu_audio.h"
#include "app.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static unsigned opened,released,outputs,nonzero;
static unsigned active;
static bool stream_owned;
static int16_t first_peak,last_sample;
static unsigned run_outputs;
int sceAudioOutOpenPort(int type,int len,int frequency,int mode)
{ (void)type;assert(len==480 && frequency==48000 && mode==0);pthread_mutex_lock(&gate);assert(!active && !stream_owned);active=1;opened++;run_outputs=0;first_peak=0;pthread_mutex_unlock(&gate);return 7; }
int sceAudioOutSetVolume(int port,int flags,const int *volume)
{ assert(port==7 && flags==3 && volume[0]==32768 && volume[1]==32768);return 0; }
int sceAudioOutOutput(int port,const void *data)
{
	assert(port==7);pthread_mutex_lock(&gate);assert(active);outputs++;
	if(data) {
		const int16_t *pcm=data;
		for(unsigned i=0;i<480;i++) { if(pcm[i]) nonzero++;if(!run_outputs && pcm[i]>first_peak) first_peak=pcm[i]; }
		last_sample=pcm[479];run_outputs++;
	}
	pthread_mutex_unlock(&gate);struct timespec delay={.tv_nsec=1000000};nanosleep(&delay,NULL);return 0;
}
int sceAudioOutReleasePort(int port)
{ assert(port==7);pthread_mutex_lock(&gate);assert(active);active=0;released++;pthread_mutex_unlock(&gate);return 0; }
static void wait_output(void)
{
	for(unsigned i=0;i<5000;i++) {
		pthread_mutex_lock(&gate);bool done=nonzero>100;pthread_mutex_unlock(&gate);if(done) return;
		struct timespec delay={.tv_nsec=1000000};nanosleep(&delay,NULL);
	}
	assert(!"audio did not produce audible output");
}

static PlusVitaAppEffect choose(PlusVitaApp *app,PlusVitaAppAction action)
{
	PlusVitaAppView view=plus_vita_app_view(app);
	for(size_t i=0;i<view.action_count;i++) if(view.actions[i].id==action)
		return plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=view.actions[i].x+10,.y=view.actions[i].y+10});
	assert(!"expected visible action");return (PlusVitaAppEffect){0};
}

static void sync_menu(PlusVitaMenuAudio *audio,PlusVitaApp *app,bool foreground)
{ plus_vita_menu_audio_update(audio,plus_vita_app_presentation(app),foreground && plus_vita_app_ambience_allowed(app)); }

static void wait_fresh_audio(void)
{
	pthread_mutex_lock(&gate);unsigned prior=nonzero;pthread_mutex_unlock(&gate);
	for(unsigned i=0;i<5000;i++) {
		pthread_mutex_lock(&gate);bool done=nonzero>prior+100;pthread_mutex_unlock(&gate);if(done) return;
		struct timespec delay={.tv_nsec=1000000};nanosleep(&delay,NULL);
	}
	assert(!"restarted menu did not become audible");
}

static void app_lifecycle(PlusVitaMenuAudio *audio)
{
	PlusVitaApp *app=plus_vita_app_create();assert(app);
	PlusVitaAppEffect load=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START});
	const char json[]="{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
	PlusVitaAppEffect auth=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,
		.operation=load.operation,.data=json,.data_size=sizeof(json)-1});
	PlusVitaCatalogEntry game={.name="Synthetic Game",.id="FAKECUSA1",.launch_id_valid=true};
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_AUTHORIZED,.operation=auth.operation,.entries=&game,.entry_count=1});
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_CATALOGUE);
	sync_menu(audio,app,true);wait_fresh_audio();
	int peak20=first_peak;
	/* Exercise real app Play transitions; a fake stream can acquire output only
	 * after synchronizing menu lifecycle has completed its drain and release. */
	const PlusVitaAppResult ends[]={PLUS_VITA_RESULT_OK,PLUS_VITA_RESULT_NETWORK_ERROR,PLUS_VITA_RESULT_CANCELLED};
	for(unsigned i=0;i<3;i++) {
		choose(app,PLUS_VITA_ACTION_GAME);
		PlusVitaAppEffect play=choose(app,PLUS_VITA_ACTION_PLAY);assert(play.kind==PLUS_VITA_EFFECT_PLAY);
		sync_menu(audio,app,true);assert(!active && opened==released && last_sample==0);
		stream_owned=true;
		plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_PROGRESS,.operation=play.operation,.play_stage=5});
		sync_menu(audio,app,true);assert(!active);
		stream_owned=false;
		if(i==2) plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
		else plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,.operation=play.operation,.status=ends[i]});
		sync_menu(audio,app,true);wait_fresh_audio();
		if(plus_vita_app_view(app).screen==PLUS_VITA_APP_DETAILS)
			plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
	}
	/* A real search-dialog operation keeps app state alive while the foreground
	 * boundary pauses audio; repeated dialog and sleep cycles must be restartable. */
	for(unsigned i=0;i<2;i++) {
		PlusVitaAppEffect search=choose(app,PLUS_VITA_ACTION_SEARCH);assert(search.kind==PLUS_VITA_EFFECT_EDIT_SEARCH);
		sync_menu(audio,app,false);assert(!active && opened==released && last_sample==0);
		plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SEARCH_ENTERED,.operation=search.operation,.status=PLUS_VITA_RESULT_CANCELLED});
		sync_menu(audio,app,true);wait_fresh_audio();
		sync_menu(audio,app,false);
		plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_INTERRUPTED,.status=PLUS_VITA_RESULT_SLEEP});
		assert(!active && opened==released && last_sample==0);
		sync_menu(audio,app,true);wait_fresh_audio();
	}
	PlusVitaPresentation preferences=plus_vita_app_presentation(app);preferences.muted=true;
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PRESENTATION_LOADED,.presentation=preferences});
	sync_menu(audio,app,true);assert(!active && opened==released);
	unsigned before=opened;sync_menu(audio,app,false);sync_menu(audio,app,true);assert(opened==before);
	preferences.muted=false;preferences.volume=40;
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PRESENTATION_LOADED,.presentation=preferences});
	sync_menu(audio,app,true);wait_fresh_audio();
	assert(first_peak>=peak20*2-2 && first_peak<=peak20*2+2);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
	choose(app,PLUS_VITA_ACTION_EXIT);choose(app,PLUS_VITA_ACTION_EXIT);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_CLOSED);
	sync_menu(audio,app,true);assert(!active && opened==released && last_sample==0);
	assert(first_peak<1000); /* Gentle first block, rather than immediate full gain. */
	plus_vita_app_destroy(app);
}
int main(void)
{
	PlusVitaMenuAudio *audio=plus_vita_menu_audio_create("vita-client/assets/audio/menu.pcm");assert(audio);
	PlusVitaPresentation preferences={.volume=20};
	plus_vita_menu_audio_update(audio,preferences,true);wait_output();
	plus_vita_menu_audio_update(audio,preferences,false);assert(!active && opened==released);
	app_lifecycle(audio);
	unsigned previous=opened;
	preferences.muted=true;plus_vita_menu_audio_update(audio,preferences,true);assert(opened==previous);
	preferences.muted=false;plus_vita_menu_audio_update(audio,preferences,true);
	plus_vita_menu_audio_update(audio,preferences,true);
	plus_vita_menu_audio_update(audio,preferences,false);assert(!active && opened==released);
	plus_vita_menu_audio_close(audio);
	assert(!plus_vita_menu_audio_create("missing-menu-audio.pcm"));
	puts("menu audio: real app Play/return/failure/cancel, dialogs, sleep, mute, fades, exclusive stream ownership and exit passed");
}
