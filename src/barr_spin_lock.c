// Created by Build Barrage
#include "barr_spin_lock.h"
#include <emmintrin.h>

void barr_spin_lock(barr_spinlock_t *lock)
{
    while (__sync_lock_test_and_set(&lock->lock, 1))
    {
        _mm_pause();  // x86: tell CPU "I'm spinning"
    }
}

void barr_spin_unlock(barr_spinlock_t *lock)
{
    __sync_lock_release(&lock->lock);
}
