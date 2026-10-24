// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static PlusVitaAppEffect choose(PlusVitaApp *app,PlusVitaAppAction action) {
 PlusVitaAppView v=plus_vita_app_view(app);
 for(size_t i=0;i<v.action_count;i++) if(v.actions[i].id==action)
  return plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=v.actions[i].x+10,.y=v.actions[i].y+10});
 assert(!"Expected visible action"); return (PlusVitaAppEffect){0};
}
int main(void) {
 PlusVitaApp *app=plus_vita_app_create();
 PlusVitaAppEffect load=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START});
 const char json[]="{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
 PlusVitaAppEffect auth=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,.operation=load.operation,.data=json,.data_size=sizeof(json)-1});
 PlusVitaCatalogEntry entries[]={ {.name="Zulu",.id="FAKECUSA2",.launch_id_valid=true}, {.name="God of War",.id="FAKECUSA1",.platform="PS4",.launch_id_valid=true}, {.name="Müller 日本",.id="Missing",.launch_id_valid=false}, {.name="Zzz fourth",.id="FAKECUSA4",.launch_id_valid=true} };
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_AUTHORIZED,.operation=auth.operation,.entries=entries,.entry_count=4,
  .store_country="CH",.store_language="de",.language="de-CH"});
 assert(strcmp(plus_vita_app_view(app).actions[0].label,"God of War")==0);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad={.left_x=255,.left_y=128,.time_us=1000}});
 assert(strcmp(plus_vita_app_view(app).actions[plus_vita_app_view(app).selected].label,"Müller 日本")==0);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad={.left_x=128,.left_y=128,.time_us=2000}});
 choose(app,PLUS_VITA_ACTION_NEXT_PAGE);
 assert(strcmp(plus_vita_app_view(app).actions[0].label,"Zzz fourth")==0);
 PlusVitaAppEffect page_search=choose(app,PLUS_VITA_ACTION_SEARCH);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SEARCH_ENTERED,.operation=page_search.operation,.status=PLUS_VITA_RESULT_CANCELLED});
 assert(plus_vita_app_view(app).actions[plus_vita_app_view(app).selected].id==PLUS_VITA_ACTION_SEARCH);
 choose(app,PLUS_VITA_ACTION_GAME);
 assert(strcmp(plus_vita_app_view(app).title,"Zzz fourth")==0);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 choose(app,PLUS_VITA_ACTION_PREVIOUS_PAGE);
 PlusVitaAppEffect search=choose(app,PLUS_VITA_ACTION_SEARCH);
 assert(search.kind==PLUS_VITA_EFFECT_EDIT_SEARCH);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SEARCH_ENTERED,.operation=search.operation,.data="GOD",.data_size=3});
 PlusVitaAppView view=plus_vita_app_view(app);
 assert(view.screen==PLUS_VITA_APP_CATALOGUE && view.action_count==2);
 assert(strcmp(view.actions[0].label,"God of War")==0);
 choose(app,PLUS_VITA_ACTION_GAME); view=plus_vita_app_view(app);
 assert(view.screen==PLUS_VITA_APP_DETAILS && strcmp(view.title,"God of War")==0);
 assert(view.actions[0].id==PLUS_VITA_ACTION_PLAY);
 PlusVitaAppEffect play=choose(app,PLUS_VITA_ACTION_PLAY);
 assert(play.kind==PLUS_VITA_EFFECT_PLAY && strcmp(play.game.id,"FAKECUSA1")==0);
 assert(strcmp(play.store_country,"CH")==0 && strcmp(play.store_language,"de")==0);
 assert(strcmp(play.language,"de-CH")==0);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_PROGRESS,.operation=play.operation,.play_stage=4});
 assert(strstr(plus_vita_app_view(app).body,"Allocating"));
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,.operation=play.operation,.status=PLUS_VITA_RESULT_OK});
 view=plus_vita_app_view(app);
 assert(view.screen==PLUS_VITA_APP_CATALOGUE && strcmp(view.actions[0].label,"God of War")==0);
 assert(strstr(view.actions[1].label,"GOD"));
 choose(app,PLUS_VITA_ACTION_GAME);
 play=choose(app,PLUS_VITA_ACTION_PLAY);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,.operation=play.operation,.status=PLUS_VITA_RESULT_OK});
 assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_CATALOGUE);
 choose(app,PLUS_VITA_ACTION_GAME);
 play=choose(app,PLUS_VITA_ACTION_PLAY);
  plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,.operation=play.operation,.status=PLUS_VITA_RESULT_STREAM_STOPPED});
  view=plus_vita_app_view(app);
  assert(view.screen==PLUS_VITA_APP_STREAM_STOPPED);
  assert(strstr(view.body,"unexpectedly"));
  assert(view.actions[0].id==PLUS_VITA_ACTION_PLAY);
  assert(view.actions[1].id==PLUS_VITA_ACTION_BACK);
  play=choose(app,PLUS_VITA_ACTION_PLAY);
  assert(play.kind==PLUS_VITA_EFFECT_PLAY && strcmp(play.game.id,"FAKECUSA1")==0);
  plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,.operation=play.operation,.status=PLUS_VITA_RESULT_STREAM_STOPPED});
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 view=plus_vita_app_view(app); assert(view.screen==PLUS_VITA_APP_CATALOGUE && view.action_count==2);
  choose(app,PLUS_VITA_ACTION_GAME);
  play=choose(app,PLUS_VITA_ACTION_PLAY);
  plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,.operation=play.operation,.status=PLUS_VITA_RESULT_NETWORK_ERROR});
  assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_DETAILS);
  plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 search=choose(app,PLUS_VITA_ACTION_SEARCH);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SEARCH_ENTERED,.operation=search.operation,.status=PLUS_VITA_RESULT_CANCELLED});
 assert(plus_vita_app_view(app).action_count==2);
 search=choose(app,PLUS_VITA_ACTION_SEARCH);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SEARCH_ENTERED,.operation=search.operation,.data="not found",.data_size=9});
 assert(strstr(plus_vita_app_view(app).title,"No search results"));
 choose(app,PLUS_VITA_ACTION_CLEAR_SEARCH);
 search=choose(app,PLUS_VITA_ACTION_SEARCH);
 const char unicode[]="Müller";
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SEARCH_ENTERED,.operation=search.operation,.data=unicode,.data_size=sizeof(unicode)-1});
 choose(app,PLUS_VITA_ACTION_GAME); view=plus_vita_app_view(app);
 assert(view.screen==PLUS_VITA_APP_DETAILS && view.actions[0].id==PLUS_VITA_ACTION_DISABLED);
 plus_vita_app_destroy(app);
 puts("catalogue: search, details, graphical Play progress/return/cancel and missing metadata passed");
}
