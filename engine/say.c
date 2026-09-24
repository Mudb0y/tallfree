/* Speaks one sentence into memory, the way cli/evv.c does, and hands the
   samples to the target: a file on the host under QEMU.

   The call sequence is evv.c's so that its output on the desktop is the
   reference: 11025 Hz, 16-bit mono, the default voice. */

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evv_abi.h"
#include "evv_port.h"

typedef struct OldInst OldInst;

enum { eciWaveformBuffer = 0 };
enum { eciDataProcessed = 1 };

OldInst *STDCALL eo_new(void);
OldInst *STDCALL eo_newEx(int32_t language);
int      STDCALL es_delete(OldInst *h);
int      STDCALL et_addText(OldInst *h, const char *text);
int      STDCALL et_synthesize(OldInst *h);
int      STDCALL ev_setOutputBuffer(OldInst *h, int32_t n, void *buf);
void     STDCALL eo_registerCallback(OldInst *h, void *cb, void *data);
void     STDCALL eo_synchronizeSynth(OldInst *h);
int      STDCALL eo_speaking(OldInst *h);
int      STDCALL eo_getAvailableLanguages(uint32_t *out, int *count);
void evvRunStaticInitialisers(void);
uint32_t sys_heap_used(void);

void target_output(const short *samples, size_t n);
void target_done(int rc);
const char *target_input(void);

#define FRAME 2048

static short frame[FRAME];
static short *samples;
static size_t nsamples, cap;

static int STDCALL on_message(OldInst *h, int msg, long param, void *data)
{
    (void)h;
    (void)data;
    if (msg == eciWaveformBuffer) {
        size_t n = (size_t)param;
        if (nsamples + n > cap) {
            short *more;
            cap = (nsamples + n) * 2 + FRAME;
            more = realloc(samples, cap * sizeof *samples);
            if (more == NULL)
                return eciDataProcessed;
            samples = more;
        }
        memcpy(samples + nsamples, frame, n * sizeof *frame);
        nsamples += n;
    }
    return eciDataProcessed;
}

void coop_fatal(const char *why)
{
    printf("engine: %s\n", why);
    exit(3);
}

const char *say_text = "Hello, I am the SP four oh four, and I can talk now.";

static int say_main(void)
{
    OldInst *h;
    uint32_t langs[16];
    int n = 16, i;

    const char *text = target_input();

    if (text == NULL)
        text = say_text;
    evv_port_start();
    evvRunStaticInitialisers();
    if (eo_getAvailableLanguages(langs, &n) || n < 1) {
        printf("engine: no languages\n");
        return 1;
    }
    h = eo_new();
    if (h == NULL)
        h = eo_newEx((int32_t)langs[0]);
    if (h == NULL) {
        printf("engine: no instance\n");
        return 1;
    }
    eo_registerCallback(h, (void *)on_message, NULL);
    if (!ev_setOutputBuffer(h, FRAME, frame)) {
        printf("engine: output buffer refused\n");
        return 1;
    }
    if (!et_addText(h, text) || !et_synthesize(h)) {
        printf("engine: text refused\n");
        return 1;
    }
    for (i = 0; i < 30000 && eo_speaking(h); i++)
        evv_sleep_ms(10);
    eo_synchronizeSynth(h);
    es_delete(h);
    evv_port_finish();

    printf("engine: %u samples, heap %lu bytes\n", (unsigned)nsamples,
           (unsigned long)sys_heap_used());
    target_output(samples, nsamples);
    return 0;
}

static jmp_buf way_out;
static int exit_code;

__attribute__((noreturn)) void engine_exit(int code)
{
    exit_code = code;
    longjmp(way_out, 1);
}

int engine_main(void)
{
    int rc;

    if (setjmp(way_out) == 0)
        rc = say_main();
    else
        rc = exit_code;
    target_done(rc);
    return rc;
}
