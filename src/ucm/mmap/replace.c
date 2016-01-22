/**
 * Copyright (C) Mellanox Technologies Ltd. 2001-2015.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "mmap.h"

#include <ucm/event/event.h>
#include <ucs/sys/compiler.h>
#include <ucs/sys/preprocessor.h>
#include <ucs/type/component.h>
#include <dlfcn.h>


#define MAP_FAILED ((void*)-1)

static __thread int ucm_mmap_reentry = 0;

/**
 * Define a replacement function to a memory-mapping function call, which calls
 * the event handler, and if event handler returns error code - calls the original
 * function.
 */
#define UCM_REPLACE_MM_FUNC(_name, _event_type, _rettype, _fail_val, ...) \
    \
    /* Call the original function using dlsym(RTLD_NEXT) */ \
    _rettype ucm_orig_##_name(UCM_FUNC_DEFINE_ARGS(__VA_ARGS__)) \
    { \
        typedef _rettype (*func_ptr_t) (__VA_ARGS__); \
        static func_ptr_t orig_func_ptr = NULL; \
        \
        if (ucs_unlikely(orig_func_ptr == NULL)) { \
            orig_func_ptr = (func_ptr_t)dlsym(RTLD_NEXT, UCS_PP_QUOTE(_name)); \
        } \
        return orig_func_ptr(UCM_FUNC_PASS_ARGS(__VA_ARGS__)); \
    } \
    \
    /* Define a symbol which goes to the replacement - in case we are loaded first */ \
    _rettype _name(UCM_FUNC_DEFINE_ARGS(__VA_ARGS__)) \
    { \
        _rettype ret; \
        \
        if (ucm_mmap_reentry) { \
            return _fail_val; /* fail on re-entry - avoid recursive events */ \
        } \
        \
        ucm_mmap_reentry = 1; \
        ret = ucm_##_name(UCM_FUNC_PASS_ARGS(__VA_ARGS__)); \
        ucm_mmap_reentry = 0; \
        return ret; \
    }

/*
 * Define argument list with given types.
 */
#define UCM_FUNC_DEFINE_ARGS(...) \
    UCS_PP_FOREACH_SEP(_UCM_FUNC_ARG_DEFINE, _, \
                       UCS_PP_ZIP((UCS_PP_SEQ(UCS_PP_NUM_ARGS(__VA_ARGS__))), \
                                  (__VA_ARGS__)))

/*
 * Pass auto-generated arguments to a function call.
 */
#define UCM_FUNC_PASS_ARGS(...) \
    UCS_PP_FOREACH_SEP(_UCM_FUNC_ARG_PASS, _, UCS_PP_SEQ(UCS_PP_NUM_ARGS(__VA_ARGS__)))


/*
 * Helpers
 */
#define _UCM_FUNC_ARG_DEFINE(_, _bundle) \
    __UCM_FUNC_ARG_DEFINE(_, UCS_PP_TUPLE_0 _bundle, UCS_PP_TUPLE_1 _bundle)
#define __UCM_FUNC_ARG_DEFINE(_, _index, _type) \
    _type UCS_PP_TOKENPASTE(arg, _index)
#define _UCM_FUNC_ARG_PASS(_, _index) \
    UCS_PP_TOKENPASTE(arg, _index)




UCM_REPLACE_MM_FUNC(mmap,  UCM_EVENT_MMAP,   void*, MAP_FAILED,
                    void*, size_t, int, int, int, off_t)
UCM_REPLACE_MM_FUNC(munmap,UCM_EVENT_MUNMAP, int,   -1,
                    void*, size_t)
UCM_REPLACE_MM_FUNC(mremap,UCM_EVENT_MREMAP, void*, MAP_FAILED,
                    void*, size_t, size_t, int)
UCM_REPLACE_MM_FUNC(shmat, UCM_EVENT_SHMAT,  void*, MAP_FAILED,
                    int, const void*, int)
UCM_REPLACE_MM_FUNC(shmdt, UCM_EVENT_SHMDT,  int,   -1,
                    const void*)
UCM_REPLACE_MM_FUNC(sbrk,  UCM_EVENT_SBRK,   void*, MAP_FAILED,
                    intptr_t)


UCS_STATIC_INIT {
    /*
     * When library is loaded, invoke these to initialize the pointers to
     * original functions.
     */
    mmap(NULL, 0, 0, 0, -1, 0);
    munmap(NULL, 0);
    mremap(NULL, 0, 0, 0);
    shmat(0, NULL, 0);
    shmdt(NULL);
    sbrk(0);
}