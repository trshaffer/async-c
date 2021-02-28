#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>

#include "async.h"


static jmp_buf async_ctx_main;
static jmp_buf async_ctx_alt;
static jmp_buf *async_continuation;
static sig_atomic_t async_handler_done;
static void (*async_start_func)(void *);
static struct async_header *async_hdr;
static sigset_t async_creat_sigs;


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

    // Magic
    start_func(hdr);

    // Unreachable
    abort();
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

    // Step 14
    return;
}

int async_main(int (*amain)(int), int argc) {
    return amain(argc);
}
