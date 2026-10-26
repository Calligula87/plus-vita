// SPDX-License-Identifier: AGPL-3.0-only
#include "branding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct plus_vita_branding_t { uint32_t *background, *splash; };

static uint32_t *load_image(const char *directory,const char *name,unsigned width,unsigned height)
{
	char path[512];
	int length=snprintf(path,sizeof(path),"%s/%s",directory,name);
	if(length<0 || (size_t)length>=sizeof(path)) return NULL;
	FILE *file=fopen(path,"rb");
	if(!file) return NULL;
	unsigned char header[16];
	bool valid=fread(header,1,sizeof(header),file)==sizeof(header)
		&& !memcmp(header,"PVIMAGE1",8);
	unsigned observed_width=0,observed_height=0;
	if(valid) for(unsigned i=0;i<4;i++) {
		observed_width|=(unsigned)header[8+i]<<(8*i);
		observed_height|=(unsigned)header[12+i]<<(8*i);
	}
	valid=valid && observed_width==width && observed_height==height;
	size_t count=(size_t)width*height;
	uint32_t *pixels=valid ? malloc(count*sizeof(*pixels)) : NULL;
	valid=pixels && fread(pixels,sizeof(*pixels),count,file)==count && fgetc(file)==EOF && !ferror(file);
	if(fclose(file)!=0) valid=false;
	if(!valid) { free(pixels); return NULL; }
	return pixels;
}

PlusVitaBranding *plus_vita_branding_load(const char *directory)
{
	if(!directory) return NULL;
	PlusVitaBranding *branding=calloc(1,sizeof(*branding));
	if(!branding) return NULL;
	branding->background=load_image(directory,"menu.pvi",960,544);
	branding->splash=load_image(directory,"splash.pvi",960,544);
	return branding;
}
const uint32_t *plus_vita_branding_background(const PlusVitaBranding *branding) { return branding ? branding->background : NULL; }
const uint32_t *plus_vita_branding_splash(const PlusVitaBranding *branding) { return branding ? branding->splash : NULL; }
void plus_vita_branding_release_splash(PlusVitaBranding *branding) { if(branding) { free(branding->splash); branding->splash=NULL; } }
void plus_vita_branding_free(PlusVitaBranding *branding) { if(branding) { free(branding->background); free(branding->splash); free(branding); } }
