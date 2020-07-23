#include "vayo_threading.h"

#ifndef WIN32
int vayo_thread_create(vayo_thread_handle *thread, void *(*start_routine)(void *), void *arg) {
	return vpthread_create(thread, NULL, start_routine, arg);
}

int vayo_thread_join(vayo_thread_handle thread) {
	return vpthread_join(thread, NULL);
}

vayo_thread_handle mosquitto_current_thread_handle() {
	return vpthread_self();
}

int vayo_thread_cancel(vayo_thread_handle thread) {
	return vpthread_cancel(thread);
}

int vayo_thread_equal(vayo_thread_handle thread_1, mosquitto_thread_handle thread_2) {
	return vpthread_equal(thread_1, thread_2);
}

int vayo_mutex_init(vayo_mutex *mutex) {
	return vpthread_mutex_init(mutex, NULL);
}

int vayo_mutex_destroy(vayo_mutex *mutex) {
	return vpthread_mutex_destroy(mutex);
}

int vayo_mutex_lock(vayo_mutex *mutex) {
	return vpthread_mutex_lock(mutex);
}

int vayo_mutex_unlock(vayo_mutex *mutex) {
	return vpthread_mutex_unlock(mutex);
}

#else


int vayo_thread_create(vayo_thread_handle *thread, void *(*start_routine)(void *), void *arg) {
	if (thread == NULL || start_routine == NULL)
		return 1;

	*thread = CreateThread(NULL, 0, start_routine, arg, 0, NULL);
	if (*thread == NULL)
		return 1;

	return 0;
}

int vayo_thread_join(vayo_thread_handle thread) {
	if (WaitForSingleObject(thread, INFINITE) == WAIT_FAILED)
		return 1;

	if (!CloseHandle(thread))
		return 2;

	return 0;
}

vayo_thread_handle vayo_current_thread_handle() {
	return GetCurrentThread();
}

int vayo_thread_cancel(vayo_thread_handle thread) {
	if (!TerminateThread(thread, 0))
		return 1;

	return 0;
}

int vayo_thread_equal(vayo_thread_handle thread_1, vayo_thread_handle thread_2) {
	if (GetThreadId(thread_1) == GetThreadId(thread_2))
		return 1;

	return 0;
}

int vayo_mutex_init(vayo_mutex *mutex) {
	if (mutex == NULL)
		return 1;

	InitializeCriticalSection(mutex);
	return 0;
}

int vayo_mutex_destroy(vayo_mutex *mutex) {
	if (mutex == NULL)
		return 1;

	DeleteCriticalSection(mutex);
	return 0;
}

int vayo_mutex_lock(vayo_mutex *mutex) {
	if (mutex == NULL)
		return 1;

	EnterCriticalSection(mutex);
	return 0;
}

int vayo_mutex_unlock(vayo_mutex *mutex) {
	if (mutex == NULL)
		return 1;

	LeaveCriticalSection(mutex);
	return 0;
}

int vpthread_rwlock_init(vpthread_rwlock_t *rwlock, const vpthread_rwlockattr_t *attr)
{
    (void)attr;
    if (rwlock == NULL)
        return 1;
    InitializeSRWLock(&(rwlock->lock));
    rwlock->exclusive = false;
    return 0;
}

int vpthread_rwlock_destroy(vpthread_rwlock_t *rwlock)
{
    (void)rwlock;
	return 0;
}

int vpthread_rwlock_rdlock(vpthread_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return 1;
    AcquireSRWLockShared(&(rwlock->lock));
	return 0;
}

int vpthread_rwlock_tryrdlock(vpthread_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return 1;
    return !TryAcquireSRWLockShared(&(rwlock->lock));
}

int vpthread_rwlock_wrlock(vpthread_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return 1;
    AcquireSRWLockExclusive(&(rwlock->lock));
    rwlock->exclusive = true;

	return 0;
}

int vpthread_rwlock_trywrlock(vpthread_rwlock_t  *rwlock)
{
    BOOLEAN ret;

    if (rwlock == NULL)
        return 1;

    ret = TryAcquireSRWLockExclusive(&(rwlock->lock));
    if (ret)
        rwlock->exclusive = true;
    return ret;
}

int vpthread_rwlock_unlock(vpthread_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return 1;

    if (rwlock->exclusive) {
        rwlock->exclusive = false;
        ReleaseSRWLockExclusive(&(rwlock->lock));
    } else {
        ReleaseSRWLockShared(&(rwlock->lock));
    }
	return 0;
}



#endif
