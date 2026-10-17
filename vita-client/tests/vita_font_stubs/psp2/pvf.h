/* Host-only external PVF contract fixture; no system font data. */
#ifndef TEST_PVF_H
#define TEST_PVF_H
#include <stdint.h>
typedef void *ScePvfLibId;
typedef void *ScePvfFontId;
typedef uint32_t ScePvfError;
typedef uint16_t ScePvfCharCode;
typedef struct { unsigned maxNumFonts; void *(*allocFunc)(void *, unsigned); void *(*reallocFunc)(void *, void *, unsigned); void (*freeFunc)(void *, void *); } ScePvfInitRec;
typedef struct { unsigned languageCode, familyCode, style; } ScePvfFontStyleInfo;
typedef struct { int horizontalAdvance64, horizontalBearingX64, horizontalBearingY64, ascender64, descender64; } TestPvfMetrics;
typedef struct { TestPvfMetrics glyphMetrics; } ScePvfCharInfo;
typedef struct { TestPvfMetrics maxIGlyphMetrics; } ScePvfFontInfo;
typedef struct { unsigned width, height; } ScePvfIrect;
typedef struct { int pixelFormat, xPos64, yPos64; ScePvfIrect rect; unsigned bytesPerLine; unsigned char *buffer; } ScePvfUserImageBufferRec;
enum { SCE_PVF_LANGUAGE_LATIN=2, SCE_PVF_LANGUAGE_J=1, SCE_PVF_FAMILY_SANSERIF=1, SCE_PVF_STYLE_REGULAR=1, SCE_PVF_USERIMAGE_DIRECT8=2 };
ScePvfLibId scePvfNewLib(ScePvfInitRec *, ScePvfError *);
int scePvfDoneLib(ScePvfLibId);
int scePvfSetEM(ScePvfLibId, float);
int scePvfSetResolution(ScePvfLibId, float, float);
int scePvfFindOptimumFont(ScePvfLibId, ScePvfFontStyleInfo *, ScePvfError *);
ScePvfFontId scePvfOpen(ScePvfLibId, int, unsigned, ScePvfError *);
int scePvfClose(ScePvfFontId);
int scePvfSetCharSize(ScePvfFontId, float, float);
int scePvfIsElement(ScePvfFontId, ScePvfCharCode);
int scePvfGetFontInfo(ScePvfFontId, ScePvfFontInfo *);
int scePvfGetCharInfo(ScePvfFontId, ScePvfCharCode, ScePvfCharInfo *);
int scePvfGetCharImageRect(ScePvfFontId, ScePvfCharCode, ScePvfIrect *);
int scePvfGetCharGlyphImage(ScePvfFontId, ScePvfCharCode, ScePvfUserImageBufferRec *);
#endif
