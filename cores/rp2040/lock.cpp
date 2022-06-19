/*
    Newlib retargetable lock class using RP2040 SDK

    Overrides weak functions in Newlib for locking to safely support
    multi-core operation.  Does not need any --wrap for memory allocators.

    Copyright (c) 2022 Earle F. Philhower, III <earlephilhower@yahoo.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <pico/mutex.h>
#include <sys/lock.h>

recursive_mutex_t __lock___sinit_recursive_mutex;
recursive_mutex_t __lock___sfp_recursive_mutex;
recursive_mutex_t __lock___atexit_recursive_mutex;
mutex_t __lock___at_quick_exit_mutex;
recursive_mutex_t __lock___malloc_recursive_mutex;
recursive_mutex_t __lock___env_recursive_mutex;
mutex_t __lock___tz_mutex;
mutex_t __lock___dd_hash_mutex;
mutex_t __lock___arc4random_mutex;


__attribute__ ((constructor)) void __init_all_newlib_mutexes() {
    recursive_mutex_init(&__lock___sinit_recursive_mutex);
    recursive_mutex_init(&__lock___sfp_recursive_mutex);
    recursive_mutex_init(&__lock___atexit_recursive_mutex);
    mutex_init(&__lock___at_quick_exit_mutex);
    recursive_mutex_init(&__lock___malloc_recursive_mutex);
    recursive_mutex_init(&__lock___env_recursive_mutex);
    mutex_init(&__lock___tz_mutex);
    mutex_init(&__lock___dd_hash_mutex);
    mutex_init(&__lock___arc4random_mutex);
}

void __retarget_lock_init(_LOCK_T *lock) {
    mutex_init((mutex_t*) lock);
}

void __retarget_lock_init_recursive(_LOCK_T *lock) {
    recursive_mutex_init((recursive_mutex_t*) lock);
}

void __retarget_lock_close(_LOCK_T lock) {
    (void) lock;
}

void __retarget_lock_close_recursive(_LOCK_T lock) {
    (void) lock;
}

void __retarget_lock_acquire(_LOCK_T lock) {
    mutex_enter_blocking((mutex_t*)lock);
}

void __retarget_lock_acquire_recursive(_LOCK_T lock) {
    recursive_mutex_enter_blocking((recursive_mutex_t*)lock);
}

int __retarget_lock_try_acquire(_LOCK_T lock) {
    return mutex_try_enter((mutex_t *)lock, NULL);
}

int __retarget_lock_try_acquire_recursive(_LOCK_T lock) {
    return recursive_mutex_try_enter((recursive_mutex_t*)lock, NULL);
}

void __retarget_lock_release(_LOCK_T lock) {
    mutex_exit((mutex_t*)lock);
}

void __retarget_lock_release_recursive(_LOCK_T lock) {
    recursive_mutex_exit((recursive_mutex_t*)lock);
}
