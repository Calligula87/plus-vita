// SPDX-License-Identifier: AGPL-3.0-only
#include "presentation.h"
#include <stdio.h>
#include <string.h>
void plus_vita_presentation_defaults(PlusVitaPresentation *preferences)
{ *preferences=(PlusVitaPresentation){.volume=20}; }
bool plus_vita_presentation_load(const char *path,PlusVitaPresentation *preferences)
{
	plus_vita_presentation_defaults(preferences);
	FILE *file=path ? fopen(path,"rb") : NULL;
	if(!file) return false;
	unsigned char record[11];
	size_t size=fread(record,1,sizeof(record),file);
	int extra=fgetc(file);
	bool valid=!ferror(file) && size==sizeof(record) && extra==EOF;
	if(fclose(file)) valid=false;
	if(!valid || memcmp(record,"PVSTYLE1",8) || record[8]>1 || record[9]>1 || record[10]>100) return false;
	*preferences=(PlusVitaPresentation){.reduced_motion=record[8]!=0,.muted=record[9]!=0,.volume=record[10]};
	return true;
}
bool plus_vita_presentation_save(const char *path,const PlusVitaPresentation *preferences)
{
	if(!path || !preferences || preferences->volume>100) return false;
	char temporary[1024];
	int length=snprintf(temporary,sizeof(temporary),"%s.tmp",path);
	if(length<0 || (size_t)length>=sizeof(temporary)) return false;
	unsigned char record[11]="PVSTYLE1";
	record[8]=preferences->reduced_motion;record[9]=preferences->muted;record[10]=(unsigned char)preferences->volume;
	FILE *file=fopen(temporary,"wb");if(!file) return false;
	bool success=fwrite(record,1,sizeof(record),file)==sizeof(record);
	if(fflush(file)) success=false;
	if(fclose(file)) success=false;
	if(success) success=rename(temporary,path)==0;
	if(!success) remove(temporary);
	return success;
}
