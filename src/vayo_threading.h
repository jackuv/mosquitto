#ifndef VAYO_THREADING_H
#define VAYO_THREADING_H

#ifndef WIN32
#  include <pthread.h>
#else
#  include <stdbool.h>
#  include <windows.h>
#endif

#ifndef WIN32
typedef vpthread_mutex_t mosquitto_mutex;
typedef vpthread_t mosquitto_thread_handle;
#else
typedef CRITICAL_SECTION vayo_mutex;
typedef HANDLE vayo_thread_handle;
typedef void vpthread_rwlockattr_t;
typedef struct {
    SRWLOCK lock;
    bool    exclusive;
} vpthread_rwlock_t;
#endif

int vayo_thread_create(vayo_thread_handle *thread, void *(*mosquitto_thread_function)(void *), void *arg);
int vayo_thread_join(vayo_thread_handle thread);
vayo_thread_handle vayo_current_thread_handle();
int vayo_thread_cancel(vayo_thread_handle thread);
int vayo_thread_equal(vayo_thread_handle thread_1, vayo_thread_handle thread_2);
int vayo_mutex_init(vayo_mutex *mutex);
int vayo_mutex_destroy(vayo_mutex *mutex);
int vayo_mutex_lock(vayo_mutex *mutex);
int vayo_mutex_unlock(vayo_mutex *mutex);

int vpthread_rwlock_init(vpthread_rwlock_t *rwlock, const vpthread_rwlockattr_t *attr);
int vpthread_rwlock_destroy(vpthread_rwlock_t *rwlock);
int vpthread_rwlock_rdlock(vpthread_rwlock_t *rwlock);
int vpthread_rwlock_tryrdlock(vpthread_rwlock_t *rwlock);
int vpthread_rwlock_wrlock(vpthread_rwlock_t *rwlock);
int vpthread_rwlock_trywrlock(vpthread_rwlock_t  *rwlock);
int vpthread_rwlock_unlock(vpthread_rwlock_t *rwlock);
#endif
