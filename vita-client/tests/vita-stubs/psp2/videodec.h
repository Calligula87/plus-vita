/* Test declarations for native boundaries; production uses VitaSDK headers. */
#pragma once
#include <stdint.h>
#define SCE_AVCDEC_PIXELFORMAT_RGBA8888 0
#define SCE_VIDEODEC_TYPE_HW_AVCDEC 0x1001
#define SCE_AVCDEC_ERROR_OUT_OF_MEMORY 0x80620003u
typedef struct { void *pBuf; uint32_t size; } SceAvcdecBuf;
typedef struct { uint32_t handle; SceAvcdecBuf frameBuf; } SceAvcdecCtrl;
typedef struct { uint32_t upper, lower; } SceVideodecTimeStamp;
typedef struct { SceVideodecTimeStamp pts, dts; SceAvcdecBuf es; } SceAvcdecAu;
typedef struct { uint32_t horizontal, vertical, numOfRefFrames; } SceAvcdecQueryDecoderInfo;
typedef struct { uint32_t frameMemSize; } SceAvcdecDecoderInfo;
typedef struct { uint32_t size, horizontal, vertical, numOfRefFrames, numOfStreams; } SceVideodecQueryInitInfoHwAvcdec;
typedef struct {
	uint32_t pixelType, framePitch, frameWidth, frameHeight, horizontalSize, verticalSize;
	struct { struct { uint8_t alpha; } rgba; } opt;
	void *pPicture[2];
} SceAvcdecFrame;
typedef struct { uint32_t size; SceAvcdecFrame frame; } SceAvcdecPicture;
typedef struct { uint32_t numOfOutput, numOfElm; SceAvcdecPicture **pPicture; } SceAvcdecArrayPicture;
int sceVideodecInitLibrary(int, const SceVideodecQueryInitInfoHwAvcdec *);
int sceVideodecTermLibrary(int);
int sceAvcdecQueryDecoderMemSize(int, const SceAvcdecQueryDecoderInfo *, SceAvcdecDecoderInfo *);
int sceAvcdecCreateDecoder(int, SceAvcdecCtrl *, const SceAvcdecQueryDecoderInfo *);
int sceAvcdecDeleteDecoder(SceAvcdecCtrl *);
int sceAvcdecDecode(SceAvcdecCtrl *, const SceAvcdecAu *, SceAvcdecArrayPicture *);
