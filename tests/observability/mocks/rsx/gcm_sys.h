/* Host mock of the few gcm calls visual_renderer uses. */
#ifndef MOCK_RSX_GCM_SYS_H
#define MOCK_RSX_GCM_SYS_H
#include <stdint.h>
typedef struct { int unused; } gcmContextData;
int gcmGetFlipStatus(void);
void gcmResetFlipStatus(void);
int gcmSetFlip(gcmContextData *context, uint32_t buffer);
void gcmSetWaitFlip(gcmContextData *context);
#endif
