#ifndef BARR_SPIN_LOCK_H_
#define BARR_SPIN_LOCK_H_

#include "barr_defs.h"

typedef struct
{
    volatile barr_i32 lock;  // 0 = free, 1 = locked
} barr_spinlock_t;

void barr_spin_lock(barr_spinlock_t *lock);
void barr_spin_unlock(barr_spinlock_t *lock);

#endif  // BARR_SPIN_LOCK_H_
