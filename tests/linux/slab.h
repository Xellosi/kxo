/* Userspace shim for <linux/slab.h> */
#pragma once

#include <linux/types.h>
#include <stdlib.h>

#define GFP_KERNEL 0

static inline void *kzalloc_shim(size_t size, int flags)
{
    (void) flags;
    return calloc(1, size);
}
#define kzalloc(size, flags) kzalloc_shim(size, flags)
#define kfree(ptr) free(ptr)
