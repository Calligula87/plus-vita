/* Test boundary only: deterministic 10ms silent codec, not an Opus quality test. */
#pragma once
#include <stdint.h>
typedef int32_t opus_int32;
typedef struct OpusDecoder { int channels; } OpusDecoder;
typedef struct OpusEncoder { int channels; } OpusEncoder;
#define OPUS_OK 0
#define OPUS_APPLICATION_AUDIO 2049
#define OPUS_RESET_STATE 4028
OpusDecoder *opus_decoder_create(opus_int32, int, int *);
OpusEncoder *opus_encoder_create(opus_int32, int, int, int *);
void opus_decoder_destroy(OpusDecoder *);
void opus_encoder_destroy(OpusEncoder *);
int opus_encode(OpusEncoder *, const int16_t *, int, unsigned char *, opus_int32);
int opus_decode(OpusDecoder *, const unsigned char *, opus_int32, int16_t *, int, int);
int opus_decoder_ctl(OpusDecoder *, int, ...);
