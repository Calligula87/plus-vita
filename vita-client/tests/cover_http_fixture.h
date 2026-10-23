#include "cover_fixture.h"
typedef size_t (*Receiver)(void *, size_t, size_t, void *);
static Receiver receive;
static curl_xferinfo_callback progress;
static void *destination;
static const atomic_bool *progress_data;
static long peer, host, redirects, timeout, connect_timeout, status=200;
static const char *ca;
static unsigned cover_requests, cover_mode;
static int handle;
CURL *curl_easy_init(void) { return (CURL *)&handle; }
void curl_easy_cleanup(CURL *curl) { assert(curl == (CURL *)&handle); }
void curl_slist_free_all(struct curl_slist *list) { if(list) { free(list->data); free(list); } }
struct curl_slist *curl_slist_append(struct curl_slist *list,const char *text)
{
 assert(!list && strstr(text,":443:127.0.0.1"));
 list=calloc(1,sizeof(*list)); assert(list); list->data=strdup(text); assert(list->data); return list;
}
int sceNetResolverCreate(const char *name,void *param,int flags)
{ (void)name; assert(!param && !flags); return 7; }
int sceNetResolverDestroy(int resolver) { assert(resolver==7); return 0; }
static atomic_bool *dns_cancel;
int sceNetResolverStartNtoa(int resolver,const char *name,SceNetInAddr *address,int timeout,int retry,int flags)
{
 assert(resolver==7 && timeout==1000000 && retry==1 && !flags);
 assert(strcmp(name,"vulcan.dl.playstation.net")==0);
 if(cover_mode==4) return -1;
 if(cover_mode==5) atomic_store(dns_cancel,true);
 address->s_addr=htonl(0x7f000001); return 0;
}
/* Only the external HTTP adapter is simulated. The real downloader owns the
 * receive buffer, limits, cancellation, response status and TLS configuration. */
CURLcode curl_easy_setopt(CURL *curl,CURLoption option,...)
{
 (void)curl; va_list args; va_start(args,option);
 switch(option) {
 case CURLOPT_URL: (void)va_arg(args,const char *); break;
 case CURLOPT_IPRESOLVE: assert(va_arg(args,long)==CURL_IPRESOLVE_V4); break;
 case CURLOPT_RESOLVE: assert(va_arg(args,struct curl_slist *)); break;
 case CURLOPT_CAINFO: ca=va_arg(args,const char *); break;
 case CURLOPT_SSL_VERIFYPEER: peer=va_arg(args,long); break;
 case CURLOPT_SSL_VERIFYHOST: host=va_arg(args,long); break;
 case CURLOPT_FOLLOWLOCATION: redirects=va_arg(args,long); break;
 case CURLOPT_CONNECTTIMEOUT_MS: connect_timeout=va_arg(args,long); break;
 case CURLOPT_TIMEOUT_MS: timeout=va_arg(args,long); break;
 case CURLOPT_NOSIGNAL: case CURLOPT_NOPROGRESS: (void)va_arg(args,long); break;
 case CURLOPT_WRITEFUNCTION: receive=va_arg(args,Receiver); break;
 case CURLOPT_WRITEDATA: destination=va_arg(args,void *); break;
 case CURLOPT_XFERINFOFUNCTION: progress=va_arg(args,curl_xferinfo_callback); break;
 case CURLOPT_XFERINFODATA: progress_data=va_arg(args,const atomic_bool *); break;
 default: assert(!"Unexpected request option");
 }
 va_end(args); return CURLE_OK;
}
CURLcode curl_easy_getinfo(CURL *curl,CURLINFO info,...)
{
 (void)curl; assert(info==CURLINFO_RESPONSE_CODE);
 va_list args; va_start(args,info); *va_arg(args,long *)=status; va_end(args); return CURLE_OK;
}
CURLcode curl_easy_perform(CURL *curl)
{
 (void)curl; cover_requests++;
 assert(peer==1 && host==2 && redirects==0 && timeout==10000 && connect_timeout==5000);
 assert(strcmp(ca,"app0:/cacert.pem")==0);
 if(cover_mode==6) return receive((void *)cover_png,1,sizeof(cover_png),destination)==sizeof(cover_png) ? CURLE_OK : CURLE_WRITE_ERROR;
 char chunk[4096]={0};
 if(cover_mode==2) { atomic_store((atomic_bool *)progress_data,true); assert(progress((void *)progress_data,0,0,0,0)); }
 if(cover_mode==3) { assert(receive(chunk,SIZE_MAX,2,destination)==0); return CURLE_WRITE_ERROR; }
 unsigned chunks=cover_mode==1 ? 513 : 1;
 for(unsigned i=0;i<chunks;i++) if(receive(chunk,1,sizeof(chunk),destination)!=sizeof(chunk)) return CURLE_WRITE_ERROR;
 return CURLE_OK;
}
