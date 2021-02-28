#ifndef ASYNC_H
#define ASYNC_H

#include <stdlib.h>
#include <setjmp.h>

// default 16KiB stack size for coroutines
#define ASYNC_STACK_SIZE (1<<14)

void *async_prepare_stack(void (*entry)(void *), void *arg, jmp_buf *continuation);
int async_main(int (*amain)(int), int argc);

// GCC extension
// https://gcc.gnu.org/onlinedocs/cpp/Duplication-of-Side-Effects.html
#define AWAIT(coro) \
    ({ \
        typeof(coro) x_async_coro_ = (coro); \
        (x_async_coro_->call)(x_async_coro_); \
    })

#define ASYNC(ident, func, ...) \
    struct X_ASYNC_CONCAT(x_async_coroutine_, func) * ident = X_ASYNC_CONCAT(x_async_create_, func) (__VA_ARGS__)

#define X_ASYNC_CONCAT(x, y) \
    X_ASYNC_CONCAT_(x, y)

#define X_ASYNC_CONCAT_(x, y) \
    x ## y

#define X_ASYNC_128TH_ARG( \
    _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
    _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
    _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
    _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
    _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
    _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
    _61,_62,_63,_64,_65,_66,_67,_68,_69,_70, \
    _71,_72,_73,_74,_75,_76,_77,_78,_79,_80, \
    _81,_82,_83,_84,_85,_86,_87,_88,_89,_90, \
    _91,_92,_93,_94,_95,_96,_97,_98,_99,_100, \
    _101,_102,_103,_104,_105,_106,_107,_108,_109,_110, \
    _111,_112,_113,_114,_115,_116,_117,_118,_119,_120, \
    _121,_122,_123,_124,_125,_126,_127,N,...) N

#define X_ASYNC_RSEQ_N() \
    127,126,125,124,123,122,121,120, \
    119,118,117,116,115,114,113,112,111,110, \
    109,108,107,106,105,104,103,102,101,100, \
    99,98,97,96,95,94,93,92,91,90, \
    89,88,87,86,85,84,83,82,81,80, \
    79,78,77,76,75,74,73,72,71,70, \
    69,68,67,66,65,64,63,62,61,60, \
    59,58,57,56,55,54,53,52,51,50, \
    49,48,47,46,45,44,43,42,41,40, \
    39,38,37,36,35,34,33,32,31,30, \
    29,28,27,26,25,24,23,22,21,20, \
    19,18,17,16,15,14,13,12,11,10, \
    9,8,7,6,5,4,3,2,1,0

#define X_ASYNC_NARGS(...) \
    X_ASYNC_NARGS_(__VA_ARGS__, X_ASYNC_RSEQ_N())

#define X_ASYNC_NARGS_(...) \
    X_ASYNC_128TH_ARG(__VA_ARGS__)

#define X_ASYNC_DEF(dispatch, rtyp, ident, ...) \
    dispatch(rtyp, ident, __VA_ARGS__)

#define ASYNC_DEF(rtyp, ident, ...) \
    X_ASYNC_DEF(X_ASYNC_CONCAT(X_ASYNC_DEF_, X_ASYNC_NARGS(__VA_ARGS__)), rtyp, ident, __VA_ARGS__)

#define X_ASYNC_DEF_2(rtyp, ident, typ0, param0) \
    rtyp ident(typ0 param0); \
    struct X_ASYNC_CONCAT(x_async_coroutine_, ident) { \
        rtyp (*call)(struct X_ASYNC_CONCAT(x_async_coroutine_, ident) *); \
        void *stack; \
        jmp_buf continuation; \
        jmp_buf caller; \
        rtyp retval; \
        typ0 arg0; \
    }; \
    void X_ASYNC_CONCAT(x_async_enter_, ident) (void *ptr) { \
        struct X_ASYNC_CONCAT(x_async_coroutine_, ident) *coro = ptr; \
        coro->retval = ident(coro->arg0); \
        longjmp(coro->caller, 1); \
    } \
    rtyp X_ASYNC_CONCAT(x_async_call_, ident) (struct X_ASYNC_CONCAT(x_async_coroutine_, ident) *coro) { \
        if (setjmp(coro->caller) == 0) { \
            longjmp(coro->continuation, 1); \
        } \
        rtyp out = coro->retval; \
        free(coro->stack); \
        free(coro); \
        return out;\
    } \
    struct X_ASYNC_CONCAT(x_async_coroutine_, ident) * X_ASYNC_CONCAT(x_async_create_, ident) (typ0 param0) { \
        struct X_ASYNC_CONCAT(x_async_coroutine_, ident) *coro = malloc(sizeof(*coro)); \
        coro->call = X_ASYNC_CONCAT(x_async_call_, ident); \
        coro->stack = async_prepare_stack(X_ASYNC_CONCAT(x_async_enter_, ident), coro, &coro->continuation); \
        coro->arg0 = param0; \
        return coro; \
    } \
    rtyp ident(typ0 param0)

#endif
