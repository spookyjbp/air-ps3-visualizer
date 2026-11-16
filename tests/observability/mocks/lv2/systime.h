/* Host mock of PSL1GHT lv2/systime.h for visual_renderer_test. */
#ifndef MOCK_LV2_SYSTIME_H
#define MOCK_LV2_SYSTIME_H
#include <stdint.h>
uint64_t sysGetSystemTime(void);
#endif
