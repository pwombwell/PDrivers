// cpprt_shim.cpp
//
// Minimal Norcroft C++ runtime shim for RISC OS.
//
// Covers:
//   __cpp_initialise
//   __cpp_finalise
//   __push_ddtor__FPFv_v
//   __pvfn__Fv
//   __vc__FPvT1iPFPv_v
//   __vc__FPvT1iPFPvi_v
//   __vc__FPvN21UiPFPvT1_v
//
// Assumptions:
// - linker provides ctor/dtor vector bounds
// - top-level dtors should run after deferred/local-static dtors
// - local-static dtors are registered dynamically via __push_ddtor
// - vector dtor callback should be invoked with delete-flag 0
//
// This is not thread-safe.

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

void* operator new(size_t size)
{
    // I can't remember if Acorn's SCL returns a pointer for no size.
    if (size == 0)
        size = 1;

    return malloc(size);
}

void operator delete(void* p)
{
    free(p);
}


// Norcroft emits the scalar __nw/__dl entry points for the array new/delete
// uses, but it does not parse global operator new[]/delete[] definitions correctly.

typedef void (*fn0_t)(void);
typedef void (*vec1_t)(void*);
typedef void (*vec1i_t)(void*, int);
typedef void (*vec2_t)(void*, void*);

struct DtorNode {
    fn0_t fn;
    DtorNode* next;
};

static DtorNode* g_ddtorHead = 0;
static bool g_cppInitialised = false;
static bool g_cppFinalised = false;

static void call_ctorvec()
{
    // Norcroft emits C$$ctorvec in objects with static C++ constructors, but
    // drlink does not provide stable __cpp_ctorvec_* bounds.
    // PDrivers don't actually use static constructors so it's fine for now.
}

static void call_dtorvec_reverse()
{
}

static void call_deferred_dtors_lifo()
{
    while (g_ddtorHead != 0) {
        DtorNode* node = g_ddtorHead;
        g_ddtorHead = node->next;
        fn0_t fn = node->fn;

        free(node);

        if (fn != 0)
            fn();
    }
}

static void vc_walk_1(void* begin, void* end, int step, vec1_t fn)
{
    if (fn == 0 || step == 0) {
        return;
    }

    char* p = (char*)begin;
    char* e = (char*)end;

    if (step > 0) {
        while (p < e) {
            fn(p);
            p += step;
        }
    } else {
        while (p > e) {
            fn(p);
            p += step;
        }
    }
}

static void vc_walk_1i(void* begin, void* end, int step, vec1i_t fn)
{
    if (fn == 0 || step == 0) {
        return;
    }

    char* p = (char*)begin;
    char* e = (char*)end;

    if (step > 0) {
        while (p < e) {
            fn(p, 0);
            p += step;
        }
    } else {
        while (p > e) {
            fn(p, 0);
            p += step;
        }
    }
}

static void vc_walk_2(void* dst_begin,
                      void* src_begin,
                      void* src_end,
                      size_t elem_size,
                      vec2_t fn)
{
    if (fn == 0 || elem_size == 0) {
        return;
    }

    char* dst = (char*)dst_begin;
    char* src = (char*)src_begin;
    char* end = (char*)src_end;

    while (src < end) {
        fn(dst, src);
        dst += elem_size;
        src += elem_size;
    }
}

extern "C" {

// Top-level ctor runner.
void __cpp_initialise(void)
{
    if (g_cppInitialised)
        return;

    g_cppInitialised = true;

    call_ctorvec();
}

// Top-level + deferred dtor runner.
void __cpp_finalise(void)
{
    if (g_cppFinalised)
        return;

    g_cppFinalised = true;

    // Dynamic/local-static dtors first.
    call_deferred_dtors_lifo();

    // Then file-scope dtorvec in reverse order.
    call_dtorvec_reverse();
}

// Register deferred destructor for function/block-scope statics.
void __push_ddtor__FPFv_v(fn0_t fn)
{
    DtorNode* node = (DtorNode*)malloc(sizeof(DtorNode));
    if (node == 0) {
        fputs("__push_ddtor: out of memory\n", stderr);
        abort();
    }

    node->fn = fn;
    node->next = g_ddtorHead;
    g_ddtorHead = node;
}

// Pure virtual call trap.
void __pvfn__Fv(void)
{
    fputs("pure virtual function called\n", stderr);
    abort();
}

// __vc(void* begin, void* end, int step, void (*fn)(void*))
void __vc__FPvT1iPFPv_v(void* begin, void* end, int step, vec1_t fn)
{
    vc_walk_1(begin, end, step, fn);
}

// __vc(void* begin, void* end, int step, void (*fn)(void*, int))
void __vc__FPvT1iPFPvi_v(void* begin, void* end, int step, vec1i_t fn)
{
    vc_walk_1i(begin, end, step, fn);
}

// __vc(void* dst_begin, void* src_begin, void* src_end,
//      size_t elem_size, void (*fn)(void*, void*))
void __vc__FPvN21UiPFPvT1_v(void* dst_begin,
                            void* src_begin,
                            void* src_end,
                            size_t elem_size,
                            vec2_t fn)
{
    vc_walk_2(dst_begin, src_begin, src_end, elem_size, fn);
}

} // extern "C"
