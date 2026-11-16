/* Host mock of the gcm calls and blit structs visual_renderer uses. */
#ifndef MOCK_RSX_GCM_SYS_H
#define MOCK_RSX_GCM_SYS_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;
typedef float f32;
typedef struct { int unused; } gcmContextData;
#define GCM_TRANSFER_LOCAL_TO_LOCAL 0
#define GCM_TRANSFER_MAIN_TO_LOCAL 1
#define GCM_TRANSFER_SURFACE 0
#define GCM_TRANSFER_CONVERSION_TRUNCATE 1
#define GCM_TRANSFER_SCALE_FORMAT_X8R8G8B8 4
#define GCM_TRANSFER_OPERATION_SRCCOPY 3
#define GCM_TRANSFER_ORIGIN_CENTER 1
#define GCM_TRANSFER_ORIGIN_CORNER 2
#define GCM_TRANSFER_INTERPOLATOR_NEAREST 0
#define GCM_TRANSFER_INTERPOLATOR_LINEAR 1
#define GCM_TRANSFER_SURFACE_FORMAT_A8R8G8B8 0xa
typedef struct {
	u32 conversion, format, operation;
	s16 clipX, clipY; u16 clipW, clipH;
	s16 outX, outY; u16 outW, outH;
	s32 ratioX, ratioY;
	u16 inW, inH, pitch;
	u8 origin, interp;
	u32 offset;
	u16 inX, inY;
} gcmTransferScale;
typedef struct { u32 format; u16 pitch; u8 _pad0[2]; u32 offset; } gcmTransferSurface;
int gcmGetFlipStatus(void);
void gcmResetFlipStatus(void);
int gcmSetFlip(gcmContextData *context, uint32_t buffer);
void gcmSetWaitFlip(gcmContextData *context);
#endif
