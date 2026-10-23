static int pixelsize=20;
static unsigned api_calls;
static int glyph_width=10;
static bool unavailable, bad_rect, bad_raster;

static unsigned char bitmap[128*8];
int sceSysmoduleLoadModule(unsigned x) { (void)x; return unavailable ? -1 : 0; }
int sceSysmoduleIsLoaded(unsigned x) { (void)x; return unavailable ? -1 : 0; }
int sceSysmoduleUnloadModule(unsigned x) { (void)x; return 0; }
ScePvfLibId scePvfNewLib(ScePvfInitRec *p, ScePvfError *e) { (void)p; *e=0; return (void *)1; }
int scePvfDoneLib(ScePvfLibId f) { (void)f; return 0; }
int scePvfSetEM(ScePvfLibId f,float x) { (void)f;(void)x; return 0; }
int scePvfSetResolution(ScePvfLibId f,float x,float y) { (void)f;(void)x;(void)y; return 0; }
int scePvfFindOptimumFont(ScePvfLibId f,ScePvfFontStyleInfo *s,ScePvfError *e) { (void)f;(void)s; *e=0; return 0; }
ScePvfFontId scePvfOpen(ScePvfLibId f,int i,unsigned m,ScePvfError *e) { (void)i;(void)m; *e=0; return f; }
int scePvfClose(ScePvfFontId f) { (void)f;return 0; }
int scePvfSetCharSize(ScePvfFontId f,float x,float y) { (void)f;(void)y; pixelsize=(int)(x*128/72+0.5f); api_calls++; return 0; }
int scePvfIsElement(ScePvfFontId f,ScePvfCharCode c) { (void)f;(void)c; api_calls++; return 1; }
int scePvfGetFontInfo(ScePvfFontId f,ScePvfFontInfo *i) { (void)f; memset(i,0,sizeof(*i)); i->maxIGlyphMetrics.ascender64=10*64; i->maxIGlyphMetrics.descender64=-2*64; api_calls++; return 0; }
int scePvfGetCharInfo(ScePvfFontId f,ScePvfCharCode c,ScePvfCharInfo *i) { (void)f; memset(i,0,sizeof(*i)); i->glyphMetrics.horizontalAdvance64=(glyph_width+2)*64; i->glyphMetrics.horizontalBearingY64=(pixelsize-2)*64; i->glyphMetrics.horizontalBearingX64=c=='j' ? -2*64 : 0; api_calls++; return 0; }
int scePvfGetCharImageRect(ScePvfFontId f,ScePvfCharCode c,ScePvfIrect *r) { (void)f;(void)c; *r=(ScePvfIrect){glyph_width,pixelsize}; api_calls++; return bad_rect ? -1 : 0; }
int scePvfGetCharGlyphImage(ScePvfFontId f,ScePvfCharCode c,ScePvfUserImageBufferRec *r) {
 (void)f; api_calls++;
 if(bad_raster) return -1;
 /* External raster contract: x/y are the pen baseline in 26.6 pixels. */
 int left=r->xPos64/64+(c=='j' ? -2 : 0), top=(r->yPos64-(pixelsize-2)*64)/64;
 for(int y=0;y<pixelsize;y++) for(int x=0;x<glyph_width;x++)
  if(left+x>=0 && left+x<(int)r->rect.width && top+y>=0 && top+y<(int)r->rect.height)
   r->buffer[(top+y)*r->bytesPerLine+left+x]=255;
 return 0;
}
PsvDebugScreenFont *psvDebugScreenGetFont(void) { static PsvDebugScreenFont f={bitmap};return &f; }

