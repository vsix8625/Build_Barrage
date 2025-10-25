#ifndef BARR_GC_H_
#define BARR_GC_H_

#include "barr_defs.h"
#include <bits/pthreadtypes.h>

typedef struct BARR_GC_Info
{
    void *ptr;
    size_t size;
    const char *fn;
    const char *file;
    barr_i32 line;
} BARR_GC_Info;

typedef struct BARR_GC_List
{
    BARR_GC_Info *items;
    size_t count;
    size_t capacity;
    pthread_mutex_t lock;
} BARR_GC_List;

// -------------------------------------------------------------------------------------
// Functions

/**
 * @brief Initializes the global garbage collector.
 *
 * Allocates and initializes internal structures,
 * and a mutex for thread-safe memory tracking.
 * Must be called **before any tracked allocations** are made.
 *
 * Thread safety: Safe to call from a single thread. Should not be called concurrently,
 * with other GC operations.
 *
 * @return true on successful initialization, false if mutex or internal allocations fail.
 */
bool BARR_gc_init(void);

/**
 * @brief Shuts down the global garbage collector.
 *
 * Frees all remaining tracked allocations and cleans up internal data structures,
 * including the GC mutex. This function is intended to be called **only at program
 * termination or when the GC is no longer needed**.
 *
 * Unlike BARR_gc_collect_all(), which merely frees the allocations but keeps the
 * GC list and mutex intact for further use, this function completely destroys
 * the GC state.
 *
 * Thread safety: The function acquires the GC mutex internally. After this call,
 * no further GC operations should be performed.
 */
void BARR_gc_shutdown(void);

/**
 * @brief Allocates memory of the specified size and tracks it in the GC list.
 *
 * If allocation fails, logs an error with the source location.
 *
 * Thread safety: Acquires the GC mutex internally, safe for multi-threaded use.
 *
 * @param size Number of bytes to allocate (minimum 1 byte if 0 is provided)
 * @param fn   Function name from which allocation was requested
 * @param file Source file name
 * @param line Line number in the source file
 * @return Pointer to the allocated memory, or NULL on failure
 */
void *BARR_gc_alloc_tracked(size_t size, const char *fn, const char *file, barr_i32 line);

/**
 * @brief Allocates zero-initialized memory for an array and tracks it in the GC list.
 *
 * Ensures that both element count and size are at least 1.
 *
 * Thread safety: Acquires the GC mutex internally, safe for multi-threaded use.
 *
 * @param n    Number of elements
 * @param size Size of each element in bytes
 * @param fn   Function name from which allocation was requested
 * @param file Source file name
 * @param line Line number in the source file
 * @return Pointer to the allocated memory, or NULL on failure
 */
void *BARR_gc_calloc_tracked(size_t n, size_t size, const char *fn, const char *file, barr_i32 line);

/**
 * @brief Resizes a previously allocated memory block and updates GC tracking.
 *
 * If the block is moved, unregisters the old pointer and registers the new one.
 * Logs an error if realloc fails.
 *
 * Thread safety: Acquires the GC mutex internally, safe for multi-threaded use.
 *
 * @param ptr  Pointer to the previously allocated memory
 * @param size New size in bytes (minimum 1 byte if 0 is provided)
 * @param fn   Function name from which allocation was requested
 * @param file Source file name
 * @param line Line number in the source file
 * @return Pointer to the resized memory, or NULL on failure
 */
void *BARR_gc_realloc_tracked(void *ptr, size_t size, const char *fn, const char *file, barr_i32 line);

/**
 * @brief Creates a duplicate of a string and tracks it in the GC list.
 *
 * Allocates memory for the copy and logs an error if allocation fails.
 *
 * Thread safety: Acquires the GC mutex internally, safe for multi-threaded use.
 *
 * @param src  Null-terminated source string
 * @param fn   Function name from which allocation was requested
 * @param file Source file name
 * @param line Line number in the source file
 * @return Pointer to the duplicated string, or NULL on failure
 */
char *BARR_gc_strdup_tracked(const char *src, const char *fn, const char *file, barr_i32 line);

/**
 * @brief Frees a previously tracked memory block and removes it from the GC list.
 *
 * Logs an error if the pointer was not found in the GC list.
 *
 * Thread safety: Acquires the GC mutex internally, safe for multi-threaded use.
 *
 * @param ptr Pointer to memory to free
 */
void BARR_gc_free_tracked(void *ptr);

/**
 * @brief Frees all currently tracked allocations in the global GC list.
 *
 * This function should only be called when you are certain that none of the,
 * tracked pointers are still in use by any thread.
 * It iterates over all allocations in the GC list and frees them, then resets the list.
 *
 * Thread safety: The function acquires the GC mutex internally,
 * so it is safe to call concurrently with other GC operations.
 * HOWEVER, freeing memory that is still in use elsewhere will cause undefined behavior.
 *
 * Use case: Typically called at program shutdown or when all tracked memory
 * is guaranteed to be unreachable.
 */
void BARR_gc_collect_all(void);

/**
 * @brief Prints a summary of all currently tracked allocations.
 *
 * Useful for debugging and detecting memory leaks. Thread-safe; acquires
 * the GC mutex internally. Does not free any memory.
 */
void BARR_gc_dump(void);

void BARR_gc_file_dump(void);

// -------------------------------------------------------------------------------------
// Macro helpers

#define BARR_gc_alloc(sz) BARR_gc_alloc_tracked((sz), __func__, __FILE__, __LINE__)
#define BARR_gc_calloc(n, s) BARR_gc_calloc_tracked((n), (s), __func__, __FILE__, __LINE__)
#define BARR_gc_realloc(p, sz) BARR_gc_realloc_tracked((p), (sz), __func__, __FILE__, __LINE__)
#define BARR_gc_strdup(s) BARR_gc_strdup_tracked((s), __func__, __FILE__, __LINE__)
#define BARR_gc_free(p) BARR_gc_free_tracked((p))

#endif  // BARR_GC_H_
