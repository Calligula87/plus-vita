// SPDX-License-Identifier: AGPL-3.0-only
/* Real UI scheduler, cover worker, downloader, decoder and painter;
 * controlled monotonic clock and external HTTP/font/device fixtures. */
#define _POSIX_C_SOURCE 200809L
#include "ui_flow.h"
#include "cover_download.h"
#include <curl/curl.h>
#include <psp2/pvf.h>
#include <psp2/net/net.h>
#include <debugScreen.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#undef curl_easy_setopt
#undef curl_easy_getinfo
#include "pvf_fixture.h"
#include "cover_http_fixture.h"
static uint32_t pixels[960*544];
static void pause_poll(void) { const struct timespec delay={.tv_nsec=1000000}; nanosleep(&delay,NULL); }
static void tick(PlusVitaUiFlow *flow,uint64_t now) { plus_vita_ui_flow_tick(flow,now); }
static void event(PlusVitaUiFlow *flow,PlusVitaAppEventKind kind,uint64_t now)
{ plus_vita_ui_flow_event(flow,(PlusVitaAppEvent){.kind=kind},now); }
static void settle(PlusVitaUiFlow *flow,const char *url,uint64_t now,PlusVitaCoverState state)
{
 for(unsigned i=0;i<5000;i++) {
  tick(flow,now);
  const PlusVitaCoverImage *image=plus_vita_covers_find(flow->covers,url);
  if(image && image->state==state) return;
  pause_poll();
 }
 assert(!"Cover did not finish");
}
static PlusVitaApp *catalogue(size_t count)
{
 PlusVitaApp *app=plus_vita_app_create();assert(app);
 PlusVitaAppEffect load=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START});
 const char json[]="{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
 PlusVitaAppEffect auth=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,
  .operation=load.operation,.data=json,.data_size=sizeof(json)-1});
 PlusVitaCatalogEntry games[12]={0};
 for(unsigned i=0;i<12;i++) {
  snprintf(games[i].name,sizeof(games[i].name),"Game %02u with a readable long title for ellipsis",i);
  snprintf(games[i].id,sizeof(games[i].id),"FAKECUSA%02u",i);
  snprintf(games[i].cover_url,sizeof(games[i].cover_url),"https://vulcan.dl.playstation.net/%u.png",i);
  games[i].launch_id_valid=true;
 }
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_AUTHORIZED,.operation=auth.operation,
  .entries=games,.entry_count=count});
 assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_CATALOGUE);
 return app;
}
static void list_sizes(PlusVitaFont *font)
{
 const size_t counts[]={0,1,7,8};
 for(unsigned test=0;test<4;test++) {
  size_t count=counts[test];PlusVitaApp *app=catalogue(count);
  PlusVitaUiFlow flow={.app=app,.dirty=true};tick(&flow,1);
  for(unsigned i=0;i<20;i++) event(&flow,PLUS_VITA_EVENT_DOWN,2+i);
  PlusVitaAppView view=plus_vita_app_view(app);
  if(!count) assert(!view.detail && view.action_count==1 && view.actions[0].id==PLUS_VITA_ACTION_BACK);
  else {
   assert(view.focus==count-1 && view.list_count==(count>7 ? 7 : count));
   assert(view.list_first==(count>7 ? 1 : 0));
  }
  assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
  if(count==8) assert(pixels[479*960+697]==0xff5bc6f6);
  plus_vita_app_destroy(app);
 }
}
int main(void)
{
 cover_mode=6;
 PlusVitaApp *app=catalogue(12);
 PlusVitaCovers *covers=plus_vita_covers_create();assert(covers);
 PlusVitaUiFlow flow={.app=app,.covers=covers,.dirty=true};
 PlusVitaFont *font=plus_vita_font_create();assert(font);
 const char *first="https://vulcan.dl.playstation.net/0.png";
 tick(&flow,1000);tick(&flow,3000999);
 assert(atomic_load(&cover_requests)==0);
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 tick(&flow,3001000);
 settle(&flow,first,3001000,PLUS_VITA_COVER_READY);
 assert(atomic_load(&cover_requests)==1);
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 assert(pixels[280*960+752]==0xff0000ff);
 /* Moving away clears the image; returning to cached art still waits 3s. */
 event(&flow,PLUS_VITA_EVENT_DOWN,3100000);event(&flow,PLUS_VITA_EVENT_UP,3200000);
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 assert(pixels[280*960+752]!=0xff0000ff);
 tick(&flow,6199999);assert(atomic_load(&cover_requests)==1);
 tick(&flow,6200000);assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 assert(pixels[280*960+752]==0xff0000ff && atomic_load(&cover_requests)==1);
 /* Rapid browsing has no neighbouring artwork activity. */
 for(unsigned i=0;i<10;i++) event(&flow,PLUS_VITA_EVENT_DOWN,6300000+i*100000);
 assert(atomic_load(&cover_requests)==1);
 PlusVitaAppView view=plus_vita_app_view(app);assert(view.focus==10 && view.list_count==7);
 /* A drag cancels activation and suspends eligibility until release. */
 tick(&flow,7400000);
 plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=240},7400001);
 plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_MOVE,.x=80,.y=400},7400002);
 tick(&flow,12000000);assert(atomic_load(&cover_requests)==1);
 plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=400},12000001);
 assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_CATALOGUE);
 tick(&flow,15000000);assert(atomic_load(&cover_requests)==1);
 view=plus_vita_app_view(app);char url[512];strcpy(url,view.detail->cover_url);
 tick(&flow,15000001);settle(&flow,url,15000001,PLUS_VITA_COVER_READY);
 assert(atomic_load(&cover_requests)==2);
 /* Details is explicit; returning restores the same focus/viewport and image. */
 size_t focus=view.focus, viewport=view.list_first;
 event(&flow,PLUS_VITA_EVENT_ACTIVATE,15100000);
 assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_DETAILS);
 tick(&flow,15300000);event(&flow,PLUS_VITA_EVENT_BACK,15300001);
 view=plus_vita_app_view(app);assert(view.focus==focus && view.list_first==viewport);
 tick(&flow,15500000);assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 assert(pixels[280*960+752]==0xff0000ff && atomic_load(&cover_requests)==2);
 /* Failure is stable while focused, with one retry after leaving and a new dwell. */
 cover_mode=0;
 event(&flow,PLUS_VITA_EVENT_DOWN,16000000);
 view=plus_vita_app_view(app);strcpy(url,view.detail->cover_url);
 tick(&flow,19000000);settle(&flow,url,19000000,PLUS_VITA_COVER_FAILED);
 assert(atomic_load(&cover_requests)==3);
 tick(&flow,39000000);assert(atomic_load(&cover_requests)==3);
 event(&flow,PLUS_VITA_EVENT_UP,40000000);event(&flow,PLUS_VITA_EVENT_DOWN,40100000);
 cover_mode=6;tick(&flow,43099999);assert(atomic_load(&cover_requests)==3);
 tick(&flow,43100000);settle(&flow,url,43100000,PLUS_VITA_COVER_READY);
 assert(atomic_load(&cover_requests)==4);
 /* An explicit new details choice starts immediately, before catalogue dwell. */
 event(&flow,PLUS_VITA_EVENT_DOWN,44000000);
 view=plus_vita_app_view(app);strcpy(url,view.detail->cover_url);
 event(&flow,PLUS_VITA_EVENT_ACTIVATE,44000001);
 settle(&flow,url,44000001,PLUS_VITA_COVER_READY);
 assert(atomic_load(&cover_requests)==5);
 tick(&flow,44200000);event(&flow,PLUS_VITA_EVENT_BACK,44200001);tick(&flow,44400000);
 /* Cancellation can remain in flight across the next selection's eligibility.
  * Navigation never joins a live worker; obsolete output is rejected. */
 cover_block=true;cover_late=true;
 event(&flow,PLUS_VITA_EVENT_DOWN,45000000);tick(&flow,48000000);
 for(unsigned i=0;i<5000 && atomic_load(&cover_requests)<6;i++) pause_poll();
 assert(atomic_load(&cover_requests)==6);
 assert(!strcmp(cover_url,"https://vulcan.dl.playstation.net/9.png"));
 event(&flow,PLUS_VITA_EVENT_DOWN,48100000);tick(&flow,51100000);
 assert(plus_vita_app_view(app).focus==10 && atomic_load(&cover_requests)==6);
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 assert(pixels[280*960+752]!=0xff0000ff);
 cover_block=false;
 settle(&flow,"https://vulcan.dl.playstation.net/10.png",51100000,PLUS_VITA_COVER_READY);
 assert(atomic_load(&cover_requests)==7);
 assert(!strcmp(cover_url,"https://vulcan.dl.playstation.net/10.png"));
 assert(!plus_vita_covers_find(covers,"https://vulcan.dl.playstation.net/9.png"));
 /* First tap on another row highlights; second tap opens its details. */
 view=plus_vita_app_view(app);
 PlusVitaAppButton row=view.actions[0];
 for(unsigned tap=0;tap<2;tap++) {
  plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=row.x+10,.y=row.y+10},52000000+tap*100);
  plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=row.x+10,.y=row.y+10},52000001+tap*100);
  assert(plus_vita_app_view(app).screen==(tap ? PLUS_VITA_APP_DETAILS : PLUS_VITA_APP_CATALOGUE));
 }
 /* Account rejection retires the account/cache even when a worker finishes late. */
 tick(&flow,53000000);event(&flow,PLUS_VITA_EVENT_ACTIVATE,53000001);
 PlusVitaAppEffect play=plus_vita_ui_flow_take(&flow);assert(play.kind==PLUS_VITA_EFFECT_PLAY);
 uint64_t epoch=plus_vita_app_view(app).account_epoch;
 plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,
  .operation=play.operation,.status=PLUS_VITA_RESULT_REJECTED},53100000);
 assert(plus_vita_app_view(app).account_epoch!=epoch);
 assert(!plus_vita_covers_find(covers,"https://vulcan.dl.playstation.net/10.png"));
 while(!plus_vita_covers_close(covers)) pause_poll();
 list_sizes(font);
 plus_vita_font_free(font);plus_vita_app_destroy(app);
 puts("preview: real transfers, 3000ms boundary, cached dwell, rapid browsing, drag and details return passed");
}
