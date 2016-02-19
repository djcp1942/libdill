/*

  Copyright (c) 2016 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#ifndef DILL_CR_INCLUDED
#define DILL_CR_INCLUDED

#include <setjmp.h>
#include <stdint.h>

#include "debug.h"
#include "list.h"
#include "slist.h"
#include "timer.h"
#include "utils.h"

/* Used to store debug info while stop() is blocked. */
struct dill_stopdata {
    int *hndls;
    int nhndls;
};

/* For now use longjmp. Replace by a different mechanism as needed. */
struct dill_ctx {
    int valid;
    sigjmp_buf jbuf;
};

#define DILL_OPAQUE_SIZE 48

struct dill_cr;

typedef void (*dill_unblock_cb)(struct dill_cr *cr);

/* The coroutine. The memory layout looks like this:

   +-------------------------------------------------------------+---------+
   |                                                      stack  | dill_cr |
   +-------------------------------------------------------------+---------+

   - dill_cr contains generic book-keeping info about the coroutine
   - stack is a standard C stack; it grows downwards (at the moment libdill
     doesn't support microarchitectures where stack grows upwards)

*/
struct dill_cr {
    /* The coroutine is stored in this list if it is not blocked and it is
       waiting to be executed. */
    struct dill_slist_item ready;
    /* If the coroutine is waiting for a deadline, it uses this timer. */
    struct dill_timer timer;

    /* When coroutine is suspended 'ctx' holds the context (registers and such),
       'unblock_cb' is a function to be called when the coroutine is moved back
       to the list of ready coroutines and 'result' is the value to be returned
       from the suspend function. */
    struct dill_ctx ctx;
    dill_unblock_cb unblock_cb;
    int result;

    /* 1 if blocking functions should return ECANCELED. */
    int canceled;
    /* If the coroutine is being canceled, the coroutine that performs
       the cancelation. */
    struct dill_cr *canceler;
    /* List of coroutines being canceled at the moment. */
    struct dill_list tocancel;
    /* A member of the above list. */
    struct dill_list_item tocancel_item;

    /* 1 if coroutine is done waiting only to be canceled. */
    int finished;
    /* Coroutine-local storage. */
    void *cls;
    /* Handle of the coroutine. */
    int hndl;
    /* Debugging info. */
    struct dill_debug_cr debug;
    /* Opaque storage for whatever data the current blocking operation
       needs to store while it is suspended. */
    uint8_t opaque[DILL_OPAQUE_SIZE];
};

/* Fake coroutine corresponding to the main thread of execution. */
extern struct dill_cr dill_main;

/* The coroutine that is running at the moment. */
extern struct dill_cr *dill_running;

/* Suspend running coroutine. Move to executing different coroutines. Once
   someone resumes this coroutine using dill_resume(), unblock_cb is
   invoked immediately. dill_suspend() returns the argument passed
   to dill_resume() function. */
int dill_suspend(dill_unblock_cb unblock_cb);

/* Schedules preiously suspended coroutine for execution. Keep in mind that
   it doesn't immediately run it, just puts it into the queue of ready
   coroutines. */
void dill_resume(struct dill_cr *cr, int result);

#endif