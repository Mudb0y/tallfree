/* What only the comparison run needs: the sentence from in.txt, and its
   samples to out.raw for compare.sh. */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "say.h"

int  sh_open(const char *path, int mode);
int  sh_write(int fd, const void *buf, int len);
int  sh_close(int fd);
int  sh_read(int fd, void *buf, int len);
int  sh_flen(int fd);
uint32_t sys_heap_used(void);

/* Samples collect here as the engine makes them and go to out.raw at the end,
   for compare.sh to hold against the desktop engine. */
static short *samples;
static size_t nsamples, cap;

void target_audio(const short *s, size_t n)
{
    if (nsamples + n > cap) {
        short *more;
        cap = (nsamples + n) * 2 + 4096;
        more = realloc(samples, cap * sizeof *samples);
        if (more == NULL)
            return;
        samples = more;
    }
    memcpy(samples + nsamples, s, n * sizeof *s);
    nsamples += n;
}

/* The text to speak, from in.txt beside the run if there is one. */
static const char *input(void)
{
    int fd = sh_open("in.txt", 0), n;
    char *t;

    if (fd < 0)
        return NULL;
    n = sh_flen(fd);
    t = malloc((size_t)n + 1);
    if (t == NULL || sh_read(fd, t, n) != n) {
        sh_close(fd);
        return NULL;
    }
    sh_close(fd);
    t[n] = 0;
    return t;
}

/* The buffer size the instrument uses, so the run shows it changes nothing. */
#define FRAME 512

int target_main(void)
{
    const char *text = input();
    int fd;

    if (text == NULL)
        text = "Hello, I am the SP four oh four, and I can talk now.";
    if (speech_open(FRAME))
        return 1;
    /* "#param N V" lines at the head of in.txt set the engine's settings. */
    while (strncmp(text, "#param ", 7) == 0) {
        char *end;
        int param = (int)strtol(text + 7, &end, 10);
        int value = (int)strtol(end, &end, 10);
        printf("param %d: %d, was %d\n", param, value, speech_param(param, value));
        text = strchr(text, '\n');
        text = text ? text + 1 : "";
    }
    if (speech_say(text))
        return 1;
    while (speech_busy())
        ;
    speech_close();
    printf("engine: %u samples, heap %lu bytes\n", (unsigned)nsamples,
           (unsigned long)sys_heap_used());
    fd = sh_open("out.raw", 4 | 1);   /* "wb" */
    if (fd < 0)
        return 1;
    sh_write(fd, samples, (int)(nsamples * sizeof *samples));
    sh_close(fd);
    return 0;
}

void target_done(int rc)
{
    (void)rc;
}

void target_enter(void) { }
void target_leave(void) { }

int target_launch(int (*body)(void))
{
    return body();
}
