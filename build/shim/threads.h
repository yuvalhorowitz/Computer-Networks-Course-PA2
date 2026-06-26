/* Local-testing shim: maps the C11 <threads.h> API that netproc.c uses onto
 * pthreads, because Apple's default toolchain ships no <threads.h>.
 *
 * This file exists ONLY so we can build/run the provided `netproc` on macOS for
 * empirical testing. It is NOT part of our submission, and `netproc.c` itself is
 * provided by the course (the grader builds on Linux, which has a real
 * <threads.h>). Activated by compiling with `-I build/shim`. */
#ifndef SHIM_THREADS_H_
#define SHIM_THREADS_H_

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

typedef pthread_t       thrd_t;
typedef pthread_mutex_t mtx_t;
typedef int (*thrd_start_t)(void*);

enum { thrd_success = 0, thrd_error = 1 };
enum { mtx_plain = 0 };

/* C11 thread funcs return int; pthreads use void*. Trampoline bridges the two. */
struct shim_thunk_ { thrd_start_t func; void* arg; };

static inline void* shim_trampoline_(void* p) {
	struct shim_thunk_ t = *(struct shim_thunk_*) p;
	free(p);
	return (void*) (intptr_t) t.func(t.arg);
}

static inline int thrd_create(thrd_t* thr, thrd_start_t func, void* arg) {
	struct shim_thunk_* t = malloc(sizeof *t);
	if (!t) return thrd_error;
	t->func = func;
	t->arg = arg;
	if (pthread_create(thr, NULL, shim_trampoline_, t) != 0) {
		free(t);
		return thrd_error;
	}
	return thrd_success;
}

static inline int thrd_join(thrd_t thr, int* res) {
	void* r;
	if (pthread_join(thr, &r) != 0) return thrd_error;
	if (res) *res = (int) (intptr_t) r;
	return thrd_success;
}

_Noreturn static inline void thrd_exit(int res) {
	pthread_exit((void*) (intptr_t) res);
}

static inline int mtx_init(mtx_t* m, int type) {
	(void) type;
	return pthread_mutex_init(m, NULL) == 0 ? thrd_success : thrd_error;
}
static inline int mtx_lock(mtx_t* m) {
	return pthread_mutex_lock(m) == 0 ? thrd_success : thrd_error;
}
static inline int mtx_unlock(mtx_t* m) {
	return pthread_mutex_unlock(m) == 0 ? thrd_success : thrd_error;
}
static inline void mtx_destroy(mtx_t* m) {
	pthread_mutex_destroy(m);
}

#endif /* SHIM_THREADS_H_ */
