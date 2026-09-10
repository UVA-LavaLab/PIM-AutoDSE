// UNUSED - not linked by the benchmark Makefile. See the note next to
// LDFLAGS_COMMON there for why. Kept for reference only.
//
// If you do link it, note that pthread_once below must run its initialiser:
// a bare `return 0` leaves libstdc++'s locale unconstructed and the first
// std::ifstream segfaults inside std::ctype<char>::ctype.

extern "C" {
    int pthread_mutex_lock(void *p) {
        return 0;
    }
    int pthread_mutex_unlock(void *p) {
        return 0;
    }
    int pthread_once(void *p, void (*f)(void)) {
        // Must actually run the initialiser exactly once - see header note.
        int *done = (int *)p;
        if (done && !*done) { *done = 1; if (f) f(); }
        return 0;
    }
    void *pthread_getspecific(unsigned int a) {
        return 0;
    }
    int pthread_setspecific(unsigned int a, const void *ptr) {
        return 0;
    }
    int pthread_key_create(unsigned int *p, void (*f)(void *)) {
        return 0;
    }
    int pthread_cond_broadcast(void *cond) {
        return 0;
    }
    int pthread_cond_signal(void *cond) {
        return 0;
    }
    int pthread_cond_destroy(void *cond) {
        return 0;
    }
    int voidimedwait(void *cond, void *mutex, void *abstime) {
        return 0;
    }
    int pthread_cond_wait(void *cond, void *mutex) {
        return 0;
    }
    int pthread_detach(int thread) {
        return 0;
    }
    int pthread_join(int thread, void **retval) {
        return 0;
    }
    int pthread_mutexattr_destroy(void *attr) {
        return 0;
    }
    int pthread_mutexattr_init(void *attr) {
        return 0;
    }
    int pthread_mutexattr_settype(void *attr, int type) {
        return 0;
    }
    int pthread_mutex_destroy(void *mutex) {
        return 0;
    }
    int pthread_mutex_init(void *mutex, void *attr) {
        return 0;
    }
    int voidrylock(void *mutex) {
        return 0;
    }
    int pthread_self(void) {
        return 0;
    }
    int pthread_mutex_trylock(void *mutex) {
        return 0;
    }
    int pthread_cond_timedwait(void *cond, void *mutex, void *abstime) {
        return 0;
    }
    int nanosleep(void *req, void *rem) {
        return 0;
    }
}
