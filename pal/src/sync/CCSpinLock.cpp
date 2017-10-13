//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <assert_only.h>
#include <CCSpinLock.hpp>
#include <errno.h>
// do not include PAL

template<bool shouldTrackThreadId>
void CCSpinLock<shouldTrackThreadId>::Reset(unsigned int spinCount)
{
    Assert(sizeof(pthread_mutex_t) <= sizeof(this->mutexPtr));

#ifdef DEBUG
    threadId = 0;
#endif
    int err;
    pthread_mutexattr_t mtconf;
    if (shouldTrackThreadId)
    {
        err = pthread_mutexattr_init(&mtconf); Assert(err == 0);
        err = pthread_mutexattr_settype(&mtconf, PTHREAD_MUTEX_RECURSIVE); Assert(err == 0);
    }

    pthread_mutex_t *mutex = (pthread_mutex_t*)this->mutexPtr;
    err = pthread_mutex_init(mutex, shouldTrackThreadId ? &mtconf : NULL); Assert(err == 0);

    if (shouldTrackThreadId)
    {
        err = pthread_mutexattr_destroy(&mtconf); Assert(err == 0);
    }
}

template<bool shouldTrackThreadId>
CCSpinLock<shouldTrackThreadId>::~CCSpinLock()
{
    pthread_mutex_t *mutex = (pthread_mutex_t*)this->mutexPtr;
    pthread_mutex_destroy(mutex);
}

template<bool shouldTrackThreadId>
void CCSpinLock<shouldTrackThreadId>::Enter()
{
    pthread_mutex_t *mutex = (pthread_mutex_t*)this->mutexPtr;
    int err = pthread_mutex_lock(mutex);
#ifdef DEBUG
    threadId = (size_t)pthread_self();
#endif
    AssertMsg(err == 0, "Mutex Enter has failed");
}

template<bool shouldTrackThreadId>
void CCSpinLock<shouldTrackThreadId>::Leave()
{
    pthread_mutex_t *mutex = (pthread_mutex_t*)this->mutexPtr;
#ifdef DEBUG
    threadId = -1;
#endif
    int err = pthread_mutex_unlock(mutex);
    AssertMsg(err == 0, "Mutex Leave has failed");
}

template<bool shouldTrackThreadId>
bool CCSpinLock<shouldTrackThreadId>::TryEnter()
{
    pthread_mutex_t *mutex = (pthread_mutex_t*)this->mutexPtr;
    int err = pthread_mutex_trylock(mutex);
    AssertMsg(err == 0 || err == EBUSY, "Mutex TryEnter has failed");

    if (err != EBUSY)
    {
#ifdef DEBUG
        threadId = (size_t)pthread_self();
#endif
        return true;
    }
    return false;
}

template<bool shouldTrackThreadId>
bool CCSpinLock<shouldTrackThreadId>::IsLocked() const
{
#ifdef DEBUG
    return threadId != -1;
#else
    return true;
#endif
}

template void CCSpinLock<false>::Enter();
template void CCSpinLock<true>::Enter();
template bool CCSpinLock<false>::TryEnter();
template bool CCSpinLock<true>::TryEnter();
template void CCSpinLock<false>::Leave();
template void CCSpinLock<true>::Leave();
template bool CCSpinLock<false>::IsLocked() const;
template bool CCSpinLock<true>::IsLocked() const;
template CCSpinLock<false>::~CCSpinLock();
template CCSpinLock<true>::~CCSpinLock();
template void CCSpinLock<true>::Reset(unsigned int spinCount);
template void CCSpinLock<false>::Reset(unsigned int spinCount);
