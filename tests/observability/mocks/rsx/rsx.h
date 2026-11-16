#ifndef MOCK_RSX_RSX_H
#define MOCK_RSX_RSX_H
#include "gcm_sys.h"
void rsxFlushBuffer(gcmContextData *context);
s32 rsxAddressToOffset(void *pointer, u32 *offset);
void rsxSetTransferScaleMode(gcmContextData *context, u8 mode, u8 surface);
void rsxSetTransferScaleSurface(gcmContextData *context, const gcmTransferScale *scale,
	const gcmTransferSurface *surface);
static inline s32 rsxGetFixedSint32(f32 f) { return (s32)(f * 1048576.0f); }
static inline u16 rsxGetFixedUint16(f32 f) { return (u16)(f * 16.0f); }
#endif
