#ifndef PLUS_VITA_TEST_NET_H
#define PLUS_VITA_TEST_NET_H
#include <arpa/inet.h>
typedef struct in_addr SceNetInAddr;
int sceNetResolverCreate(const char *,void *,int);
int sceNetResolverStartNtoa(int,const char *,SceNetInAddr *,int,int,int);
int sceNetResolverDestroy(int);
#endif
