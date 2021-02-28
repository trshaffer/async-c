#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>

#include "async.h"
#include "itable.h"
#include "set.h"


static jmp_buf async_ctx_main;
static jmp_buf async_ctx_alt;
static jmp_buf *async_continuation;
static sig_atomic_t async_handler_done;
static void (*async_start_func)(void *);
static struct async_header *async_hdr;
static sigset_t async_creat_sigs;

static struct async_header *async_current_coroutine;
static jmp_buf async_event_loop;

static struct set *async_ready_list;
static struct set *async_done_list;
static struct itable *async_waiters;

static int async_outstanding() {
    return set_size(async_ready_list) \
        + set_size(async_done_list) \
        + itable_size(async_waiters);
}

static void async_boot(void) {
    void (*start_func) (void *);
    void *hdr;

    // Step 10
    sigprocmask(SIG_SETMASK, &async_creat_sigs, NULL);

    // Step 11
    start_func = async_start_func;
    hdr = async_hdr;

    // Step 12 & 13
    if (setjmp(*async_continuation) == 0) {
        longjmp(async_ctx_main, 1);
    }

    // Magic happens here
    start_func(hdr);

    // Back to the event loop
    set_insert(async_done_list, hdr);
    async_current_coroutine = NULL;
    longjmp(async_event_loop, 1);
}

static void async_trampoline(int sig) {
    // Step 5
    if (setjmp(async_ctx_alt) == 0) {
        async_handler_done = true;
        return;
    }

    // Step 9
    async_boot();
}

void async_prepare_stack(void (*start_func)(void *), struct async_header *hdr) {
    struct sigaction tsa;
    struct sigaction osa;
    stack_t tss;
    stack_t oss;
    sigset_t tsigs;
    sigset_t osigs;

    hdr->stack = malloc(ASYNC_STACK_SIZE);
    if (hdr->stack == NULL) {
        abort();
    }

    // Step 1
    sigemptyset(&tsigs);
    sigaddset(&tsigs, SIGUSR1);
    sigprocmask(SIG_BLOCK, &tsigs, &osigs);

    // Step 2
    memset((void *) &tsa, 0, sizeof(struct sigaction));
    tsa.sa_handler = async_trampoline;
    tsa.sa_flags = SA_ONSTACK;
    sigaction(SIGUSR1, &tsa, &osa);

    // Step 3
    tss.ss_sp = hdr->stack;
    tss.ss_size = ASYNC_STACK_SIZE;
    tss.ss_flags = 0;
    sigaltstack(&tss, &oss);

    // Step 4
    async_continuation = &hdr->continuation;
    async_hdr = hdr;
    async_start_func = start_func;
    async_creat_sigs = osigs;
    async_handler_done = false;
    kill(getpid(), SIGUSR1);
    sigfillset(&tsigs);
    sigdelset(&tsigs, SIGUSR1);
    while (!async_handler_done) {
        sigsuspend(&tsigs);
    }

    // Step 6
    sigaltstack(NULL, &tss);
    tss.ss_flags = SS_DISABLE;
    sigaltstack(&tss, NULL);
    if (!(oss.ss_flags & SS_DISABLE)) {
        sigaltstack(&oss, NULL);
    }
    sigaction(SIGUSR1, &osa, NULL);
    sigprocmask(SIG_SETMASK, &osigs, NULL);

    // Steps 7 & 8
    if (setjmp(async_ctx_main) == 0) {
        longjmp(async_ctx_alt, 1);
    }
}

void async_yield(struct async_header *hdr) {
    struct async_header *saved = async_current_coroutine;
    assert(saved != NULL);

    set_insert(async_ready_list, hdr);
    itable_insert(async_waiters, (uintptr_t) hdr, saved);
    async_current_coroutine = NULL;

    if (setjmp(saved->continuation) == 0) {
        longjmp(async_event_loop, 1);
    }

    assert(async_current_coroutine == saved);
}

void async_free(struct async_header *hdr) {
    if (hdr == NULL) return;
    free(hdr->stack);
    free(hdr);
}

void async_main(struct async_header *hdr) {
    struct async_header *done;
    struct async_header *blocked;

    async_ready_list = set_create(0);
    async_done_list = set_create(0);
    async_waiters = itable_create(0);

    set_insert(async_ready_list, hdr);
    async_current_coroutine = hdr;

    while (async_outstanding() > 0) {
        assert(async_current_coroutine);
        set_remove(async_ready_list, async_current_coroutine);
        if (setjmp(async_event_loop) == 0) {
            longjmp(async_current_coroutine->continuation, 1);
        }
        assert(async_current_coroutine == NULL);

        set_first_element(async_ready_list);
        set_first_element(async_done_list);
        if ((async_current_coroutine = set_next_element(async_ready_list))) {
            continue;
        } else if ((done = set_next_element(async_done_list))) {
            set_remove(async_done_list, done);
            blocked = itable_remove(async_waiters, (uintptr_t) done);
            if (blocked == NULL) {
                continue;
            }
            set_insert(async_ready_list, blocked);
            async_current_coroutine = blocked;
            continue;
        }

        // unreachable, every jump back will have a new ready or done entry
        abort();
    }
}
