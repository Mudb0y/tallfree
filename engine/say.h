#ifndef TALLFREE_SAY_H
#define TALLFREE_SAY_H

#include <stddef.h>

/* The speaker, in say.c. */
int  speech_open(int frame_samples);
int  speech_say(const char *text);
int  speech_busy(void);
void speech_close(void);
int  speech_param(int param, int value);

/* What each target supplies. */
int  target_main(void);
void target_audio(const short *samples, size_t n);
void target_done(int rc);

#endif
