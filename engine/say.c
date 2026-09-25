/* The engine as a speaker: one instance, opened once, that says phrase after
   phrase and hands each buffer of samples to the target as it is made.

   The call sequence is cli/evv.c's so that the desktop's output is the
   reference: 11025 Hz, 16-bit mono, the default voice. What is done with the
   phrases is the target's: under QEMU one sentence goes to a file, on the
   instrument a service speaks what the screen draws. */

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evv_abi.h"
#include "evv_port.h"
#include "say.h"

typedef struct OldInst OldInst;

enum { eciWaveformBuffer = 0 };
enum { eciDataProcessed = 1 };

OldInst *STDCALL eo_new(void);
OldInst *STDCALL eo_newEx(int32_t language);
int      STDCALL es_delete(OldInst *h);
int      STDCALL et_addText(OldInst *h, const char *text);
int      STDCALL et_synthesize(OldInst *h);
int      STDCALL ev_setOutputBuffer(OldInst *h, int32_t n, void *buf);
int      STDCALL ev_setParam(OldInst *h, int32_t param, int32_t value);
void     STDCALL eo_registerCallback(OldInst *h, void *cb, void *data);
void     STDCALL eo_synchronizeSynth(OldInst *h);
int      STDCALL eo_speaking(OldInst *h);
int      STDCALL eo_getAvailableLanguages(uint32_t *out, int *count);
void evvRunStaticInitialisers(void);

#define FRAME_MAX 4096

static short frame[FRAME_MAX];
static OldInst *inst;

static int STDCALL on_message(OldInst *h, int msg, long param, void *data)
{
    (void)h;
    (void)data;
    if (msg == eciWaveformBuffer)
        target_audio(frame, (size_t)param);
    return eciDataProcessed;
}

void coop_fatal(const char *why)
{
    printf("engine: %s\n", why);
    exit(3);
}

int speech_open(int frame_samples)
{
    uint32_t langs[16];
    int n = 16;

    if (frame_samples < 64 || frame_samples > FRAME_MAX)
        frame_samples = FRAME_MAX;
    evv_port_start();
    evvRunStaticInitialisers();
    if (eo_getAvailableLanguages(langs, &n) || n < 1) {
        printf("engine: no languages\n");
        return -1;
    }
    inst = eo_new();
    if (inst == NULL)
        inst = eo_newEx((int32_t)langs[0]);
    if (inst == NULL) {
        printf("engine: no instance\n");
        return -1;
    }
    eo_registerCallback(inst, (void *)on_message, NULL);
    if (!ev_setOutputBuffer(inst, frame_samples, frame)) {
        printf("engine: output buffer refused\n");
        return -1;
    }
    return 0;
}

int speech_say(const char *text)
{
    if (!et_addText(inst, text) || !et_synthesize(inst)) {
        printf("engine: text refused\n");
        return -1;
    }
    return 0;
}

/* Lets the synthesis context run, and answers whether anything of the
   phrases handed over is still to come. */
int speech_busy(void)
{
    if (eo_speaking(inst)) {
        evv_sleep_ms(1);
        return 1;
    }
    eo_synchronizeSynth(inst);
    return 0;
}

/* One of the engine's eighteen settings, by eci.h's numbers; answers what it
   was, or -1 if refused. */
int speech_param(int param, int value)
{
    return inst != NULL ? ev_setParam(inst, param, value) : -1;
}

void speech_close(void)
{
    if (inst != NULL)
        es_delete(inst);
    inst = NULL;
    evv_port_finish();
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
        rc = target_main();
    else
        rc = exit_code;
    target_done(rc);
    return rc;
}
