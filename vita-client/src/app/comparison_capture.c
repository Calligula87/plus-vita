// SPDX-License-Identifier: AGPL-3.0-only
#include "comparison_capture.h"
#include <stdio.h>
bool plus_vita_comparison_capture(const uint32_t *pixels,int pitch,const char *path)
{
	if(!pixels || pitch<960 || !path) return false;
	FILE *file=fopen(path,"wb"); if(!file) return false;
	bool ok=fputs("P6\n960 544\n255\n",file)>=0;
	unsigned char row[960*3];
	for(unsigned y=0;y<544 && ok;y++) {
		for(unsigned x=0;x<960;x++) {
			uint32_t color=pixels[y*(unsigned)pitch+x];
			row[x*3]=(unsigned char)color; row[x*3+1]=(unsigned char)(color>>8); row[x*3+2]=(unsigned char)(color>>16);
		}
		ok=fwrite(row,1,sizeof(row),file)==sizeof(row);
	}
	if(fclose(file)!=0) ok=false;
	if(!ok) remove(path);
	return ok;
}
