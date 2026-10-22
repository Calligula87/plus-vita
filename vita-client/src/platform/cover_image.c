// SPDX-License-Identifier: AGPL-3.0-only
#include "cover_download.h"
#include <stdlib.h>
#include <pthread.h>
typedef union { max_align_t alignment; size_t bytes; } Allocation;
static size_t allocated;
static pthread_mutex_t memory_mutex=PTHREAD_MUTEX_INITIALIZER;
static void *image_alloc(size_t bytes)
{
	pthread_mutex_lock(&memory_mutex);
	if(bytes>8*1024*1024u-allocated) { pthread_mutex_unlock(&memory_mutex); return NULL; }
	Allocation *p=malloc(sizeof(*p)+bytes);
	if(p) { p->bytes=bytes; allocated+=bytes; }
	pthread_mutex_unlock(&memory_mutex);
	return p ? p+1 : NULL;
}
static void image_free(void *memory)
{
	if(!memory) return;
	Allocation *p=(Allocation *)memory-1;
	pthread_mutex_lock(&memory_mutex); allocated-=p->bytes; pthread_mutex_unlock(&memory_mutex);
	free(p);
}
static void *image_realloc(void *memory,size_t bytes)
{
	if(!memory) return image_alloc(bytes);
	Allocation *p=(Allocation *)memory-1;
	pthread_mutex_lock(&memory_mutex);
	if(bytes>8*1024*1024u-(allocated-p->bytes)) { pthread_mutex_unlock(&memory_mutex); return NULL; }
	size_t old=p->bytes; Allocation *grown=realloc(p,sizeof(*p)+bytes);
	if(grown) { grown->bytes=bytes; allocated=allocated-old+bytes; }
	pthread_mutex_unlock(&memory_mutex);
	return grown ? grown+1 : NULL;
}
#define STBI_MALLOC image_alloc
#define STBI_REALLOC image_realloc
#define STBI_FREE image_free
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_FAILURE_STRINGS
#define STBI_MAX_DIMENSIONS 1024
#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb_image.h"

bool plus_vita_cover_decode(const unsigned char *data,size_t size,uint32_t pixels[128*128])
{
	if(!data || !size || size>2*1024*1024u) return false;
	int width=0,height=0,channels=0;
	if(!stbi_info_from_memory(data,(int)size,&width,&height,&channels) || width<1 || height<1 || width>1024 || height>1024) return false;
	unsigned char *rgba=stbi_load_from_memory(data,(int)size,&width,&height,&channels,4);
	if(!rgba) return false;
	int w=width>=height ? 128 : width*128/height, h=height>=width ? 128 : height*128/width;
	if(w<1) w=1;
	if(h<1) h=1;
	for(unsigned i=0;i<128*128;i++) pixels[i]=0xff421d09;
	for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
		unsigned char *p=rgba+((y*height/h)*width+x*width/w)*4;
		unsigned a=p[3]; uint32_t color=0xff000000;
		const unsigned bg[]={9,29,66};
		for(unsigned c=0;c<3;c++) color|=((p[c]*a+bg[c]*(255-a))/255)<<(c*8);
		pixels[(y+(128-h)/2)*128+x+(128-w)/2]=color;
	}
	stbi_image_free(rgba);
	return true;
}
