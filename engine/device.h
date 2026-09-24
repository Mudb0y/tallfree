#ifndef TALLFREE_DEVICE_H
#define TALLFREE_DEVICE_H

#include <stddef.h>
#include <stdint.h>

/* device.c */
void engine_install(void);
uint32_t device_ticks(void);                 /* audio interrupts, 750 a second */
void target_sleep(int ms);
void target_wake(void);
void target_checkpoint(void);
void target_volume(uint32_t percent);
void target_probe_slots(void);
int  engine_task_id(void);
void write_file(const char *path, const void *buf, size_t len);
const char *read_text(const char *path);
void audio_push(const int16_t *s, size_t n);
size_t audio_space(void);
size_t audio_pending(void);
void audio_flush(void);
int  audio_hooked(void);

/* screen.c */
enum { SCREEN_OFF, SCREEN_CHANGED, SCREEN_ALL };
void request_unload(void);
int  unload_requested(void);
int  screen_install(int mode, int settle_ms);
void screen_remove(void);
int  screen_poll(char *phrases, size_t cap, int *count);
int  screen_log_write(void);
void screen_report(void);

#define TICKS_TO_MS(t) ((uint32_t)(t) * 4u / 3u)
#define MS_TO_TICKS(m) ((uint32_t)(m) * 3u / 4u)

#endif
