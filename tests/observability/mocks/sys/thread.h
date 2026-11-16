/* Host mock of PSL1GHT sys/thread.h backed by pthreads. */
#ifndef MOCK_SYS_THREAD_H
#define MOCK_SYS_THREAD_H
#include <stdint.h>
typedef uint64_t sys_ppu_thread_t;
#define THREAD_JOINABLE 1
int sysThreadCreate(sys_ppu_thread_t *id, void (*entry)(void *), void *arg,
	int priority, uint64_t stack, uint64_t flags, const char *name);
int sysThreadJoin(sys_ppu_thread_t id, uint64_t *retval);
void sysThreadExit(uint64_t value);
#endif
