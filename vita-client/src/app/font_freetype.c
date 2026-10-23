// SPDX-License-Identifier: AGPL-3.0-only
#include "font_freetype.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FT_HEAP_LIMIT (4u*1024u*1024u)
#define FONT_FILE_LIMIT (1024u*1024u)
#define CACHE_LIMIT (512u*1024u)
#define CACHE_SLOTS 256u
typedef union { max_align_t alignment; size_t bytes; } Allocation;
typedef struct {
	unsigned code;
	int size;
	bool fallback;
	PlusVitaGlyph glyph;
	unsigned char *alpha;
} Glyph;
struct plus_vita_ft_font_t {
	struct FT_MemoryRec_ memory;
	FT_Library library;
	FT_Face face;
	unsigned char *file;
	size_t heap_bytes, cache_bytes;
	unsigned next;
	int size;
	Glyph cache[CACHE_SLOTS];
	PlusVitaTextFont fallback;
	uint32_t *pixels, color;
	int pitch;
};
static void *allocate(FT_Memory memory, long amount)
{
	PlusVitaFtFont *font=memory->user;
	if(amount<=0 || (size_t)amount>FT_HEAP_LIMIT-font->heap_bytes) return NULL;
	Allocation *block=malloc(sizeof(*block)+(size_t)amount);
	if(!block) return NULL;
	block->bytes=(size_t)amount; font->heap_bytes+=block->bytes; return block+1;
}
static void release(FT_Memory memory, void *value)
{
	if(!value) return;
	Allocation *block=(Allocation *)value-1;
	PlusVitaFtFont *font=memory->user; font->heap_bytes-=block->bytes; free(block);
}
static void *resize_memory(FT_Memory memory, long old_size, long amount, void *value)
{
	(void)old_size;
	if(!value) return allocate(memory,amount);
	if(amount<=0) { release(memory,value); return NULL; }
	PlusVitaFtFont *font=memory->user; Allocation *block=(Allocation *)value-1;
	if((size_t)amount>FT_HEAP_LIMIT-(font->heap_bytes-block->bytes)) return NULL;
	size_t old=block->bytes;
	Allocation *grown=realloc(block,sizeof(*block)+(size_t)amount);
	if(!grown) return NULL;
	grown->bytes=(size_t)amount; font->heap_bytes=font->heap_bytes-old+grown->bytes; return grown+1;
}
void plus_vita_freetype_free(PlusVitaFtFont *font)
{
	if(!font) return;
	for(unsigned i=0;i<CACHE_SLOTS;i++) free(font->cache[i].alpha);
	if(font->face) FT_Done_Face(font->face);
	if(font->library) FT_Done_Library(font->library);
	free(font->file); free(font);
}
PlusVitaFtFont *plus_vita_freetype_create(const char *path, PlusVitaTextFont fallback)
{
	if(!path) return NULL;
	FILE *file=fopen(path,"rb"); if(!file) return NULL;
	PlusVitaFtFont *font=calloc(1,sizeof(*font));
	long length=-1;
	if(font && fseek(file,0,SEEK_END)==0) length=ftell(file);
	bool ok=length>0 && (unsigned long)length<=FONT_FILE_LIMIT && fseek(file,0,SEEK_SET)==0;
	if(ok) { font->file=malloc((size_t)length); ok=font->file && fread(font->file,1,(size_t)length,file)==(size_t)length; }
	if(fclose(file)!=0) ok=false;
	if(!ok) { plus_vita_freetype_free(font); return NULL; }
	font->fallback=fallback;
	font->memory=(struct FT_MemoryRec_){.user=font,.alloc=allocate,.free=release,.realloc=resize_memory};
	if(FT_New_Library(&font->memory,&font->library)) { plus_vita_freetype_free(font); return NULL; }
	FT_Add_Default_Modules(font->library);
	if(FT_New_Memory_Face(font->library,font->file,length,0,&font->face)
		|| FT_Select_Charmap(font->face,FT_ENCODING_UNICODE)) { plus_vita_freetype_free(font); return NULL; }
	return font;
}
static bool size_font(PlusVitaFtFont *font,int size)
{
	if(size<1 || size>64) return false;
	if(font->size!=size) {
		if(FT_Set_Pixel_Sizes(font->face,0,(FT_UInt)size)) return false;
		font->size=size;
	}
	return true;
}
static void evict(PlusVitaFtFont *font,Glyph *entry)
{
	if(entry->alpha) font->cache_bytes-=(size_t)entry->glyph.width*entry->glyph.height;
	free(entry->alpha); memset(entry,0,sizeof(*entry));
}
static Glyph *resolve(PlusVitaFtFont *font,unsigned code,int size)
{
	for(unsigned i=0;i<CACHE_SLOTS;i++) if(font->cache[i].code==code && font->cache[i].size==size) return &font->cache[i];
	Glyph *entry=&font->cache[font->next++%CACHE_SLOTS]; evict(font,entry);
	entry->code=code; entry->size=size;
	if(!size_font(font,size) || (!FT_Get_Char_Index(font->face,code) && font->fallback.measure)) goto fallback;
	if(!FT_Get_Char_Index(font->face,code)) code='?';
	if(FT_Load_Char(font->face,code,FT_LOAD_RENDER|FT_LOAD_TARGET_NORMAL)) goto fallback;
	FT_GlyphSlot slot=font->face->glyph; FT_Bitmap *image=&slot->bitmap;
	if(image->width>128 || image->rows>128 || (image->width && image->pixel_mode!=FT_PIXEL_MODE_GRAY)) goto fallback;
	entry->glyph=(PlusVitaGlyph){.advance=(int)((slot->advance.x+32)/64),.left=slot->bitmap_left,
		.top=slot->bitmap_top,.width=(int)image->width,.height=(int)image->rows};
	size_t bytes=(size_t)image->width*image->rows;
	if(!bytes) return entry;
	while(font->cache_bytes+bytes>CACHE_LIMIT) {
		Glyph *victim=&font->cache[font->next++%CACHE_SLOTS]; if(victim!=entry) evict(font,victim);
	}
	entry->alpha=malloc(bytes); if(!entry->alpha) goto fallback;
	font->cache_bytes+=bytes;
	for(unsigned y=0;y<image->rows;y++) {
		const unsigned char *row=image->buffer+(image->pitch>=0 ? (int)y : (int)(image->rows-1-y))*abs(image->pitch);
		memcpy(entry->alpha+y*image->width,row,image->width);
	}
	return entry;
fallback:
	entry->fallback=true;
	entry->glyph=font->fallback.measure ? font->fallback.measure(font->fallback.context,code,size)
		: (PlusVitaGlyph){.advance=size/2};
	return entry;
}
static PlusVitaGlyph measure(void *context,unsigned code,int size) { return resolve(context,code,size)->glyph; }
static void metrics(void *context,int size,int *ascent,int *descent)
{
	PlusVitaFtFont *font=context; *ascent=size; *descent=size/4;
	if(size_font(font,size)) {
		*ascent=(int)((font->face->size->metrics.ascender+63)/64);
		*descent=(int)((-font->face->size->metrics.descender+63)/64);
	}
}
static void draw(void *context,unsigned code,int x,int baseline,int size,const PlusVitaTextBox *clip)
{
	PlusVitaFtFont *font=context; Glyph *entry=resolve(font,code,size);
	if(entry->fallback) {
		if(font->fallback.draw) font->fallback.draw(font->fallback.context,code,x,baseline,size,clip);
		return;
	}
	for(int row=0;row<entry->glyph.height;row++) for(int col=0;col<entry->glyph.width;col++) {
		int px=x+entry->glyph.left+col, py=baseline-entry->glyph.top+row;
		if(px<clip->x || px>=clip->x+clip->width || py<clip->y || py>=clip->y+clip->height
			|| px<0 || px>=960 || py<0 || py>=544) continue;
		unsigned alpha=entry->alpha[row*entry->glyph.width+col]; if(!alpha) continue;
		uint32_t *target=&font->pixels[py*font->pitch+px],color=0xff000000;
		for(unsigned shift=0;shift<24;shift+=8)
			color|=(((*target>>shift&255)*(255-alpha)+(font->color>>shift&255)*alpha)/255)<<shift;
		*target=color;
	}
}
bool plus_vita_freetype_draw(PlusVitaFtFont *font,uint32_t *pixels,int pitch,const char *text,
	PlusVitaTextBox box,int size,bool wrap,uint32_t color)
{
	if(!font || !pixels || pitch<960) return false;
	font->pixels=pixels; font->pitch=pitch; font->color=color;
	PlusVitaTextFont adapter={font,metrics,measure,draw};
	return plus_vita_text_draw(&adapter,text,box,size,wrap);
}
