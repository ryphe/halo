#ifndef HALO_AUDIO_H
#define HALO_AUDIO_H

#include "halo_engine.h"

#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>

#define STREAM_BUF_SIZE     512
#define STREAM_NUM_BUFS     4
#define SCOPE_RING_SIZE     4096   /* power of two; ~93 ms at 44.1 kHz */

typedef struct {
    HWAVEOUT         hWaveOut;
    HANDLE           hEvent;
    HANDLE           hThread;
    CRITICAL_SECTION cs;
    volatile int     running;
    int              initialized;
    int              is_float_format;
    int              init_error;     /* MMRESULT of a failed waveOutOpen, 0 = none */

    float            master_volume;   /* 0..1, applied post-mix, pre-output */

    WAVEHDR          headers[STREAM_NUM_BUFS];
    float            float_buffers[STREAM_NUM_BUFS][STREAM_BUF_SIZE * 2];  /* interleaved L/R */
    short            pcm16_buffers[STREAM_NUM_BUFS][STREAM_BUF_SIZE * 2];

    HaloVoiceManager vm;
    HaloPatch        patch;

    float            scope_ring[SCOPE_RING_SIZE];
    int              scope_write_idx;
    volatile float   level;      /* peak envelope of output, drives UI logo */

    DWORD            reset_kill_tick;        /* tick when all voices must be killed */
    DWORD            audition_release_tick;  /* tick when audition notes must release */
} HaloAudio;

static HaloAudio g_audio = {0};

/* Scope tap: phase-averaged stereo downmix (Task 4.3) so L/R cancellations
   don't break the displayed waveform. Also feeds a fast peak follower used
   by the UI (animated logo opacity). */
static void audio_write_scope_cache(const float* src, int frames) {
    float peak = g_audio.level * 0.90f;   /* gentle release between blocks */
    for (int i = 0; i < frames; i++) {
        float mono = (src[2 * i] + src[2 * i + 1]) * 0.5f;
        float mag = fabsf(mono);
        if (mag > peak) peak = mag;
        g_audio.scope_ring[g_audio.scope_write_idx] = mono;
        g_audio.scope_write_idx = (g_audio.scope_write_idx + 1) & (SCOPE_RING_SIZE - 1);
    }
    if (peak > 1.0f) peak = 1.0f;
    g_audio.level = peak;
}

static float audio_get_level(void) {
    return g_audio.level;
}

/* Apply the master volume control to a rendered stereo block (monitor path).
   The safety clamp keeps the stream inside full scale for the DAC. */
static void audio_apply_master_volume(float* buf, int frames) {
    float vol = g_audio.master_volume;
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    if (vol >= 0.999f) return;   /* unity: nothing to do */
    for (int i = 0; i < frames * 2; i++) {
        buf[i] *= vol;
    }
}

static DWORD WINAPI audio_thread_proc(LPVOID param) {
    (void)param;
    float mix_scratch[STREAM_BUF_SIZE * 2];   /* interleaved stereo */

    while (g_audio.running) {
        WaitForSingleObject(g_audio.hEvent, 40);
        if (!g_audio.running) break;

        for (int i = 0; i < STREAM_NUM_BUFS; i++) {
            if (g_audio.headers[i].dwFlags & WHDR_DONE) {
                waveOutUnprepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));

                EnterCriticalSection(&g_audio.cs);

                DWORD now = GetTickCount();

                /* Auto-release audition chord (300 ms) independent of UI timers */
                if (g_audio.audition_release_tick && (long)(now - g_audio.audition_release_tick) >= 0) {
                    g_audio.audition_release_tick = 0;
                    for (int v = 0; v < HALO_MAX_VOICES; v++) {
                        if (g_audio.vm.voices[v].active &&
                            (g_audio.vm.voices[v].note == 60 ||
                             g_audio.vm.voices[v].note == 64 ||
                             g_audio.vm.voices[v].note == 67)) {
                            voice_note_off(&g_audio.vm.voices[v]);
                        }
                    }
                }

                /* Clear all voices automatically 2 seconds after reset */
                if (g_audio.reset_kill_tick && (long)(now - g_audio.reset_kill_tick) >= 0) {
                    g_audio.reset_kill_tick = 0;
                    for (int v = 0; v < HALO_MAX_VOICES; v++) {
                        voice_force_idle(&g_audio.vm.voices[v]);
                    }
                }

                halo_process_audio(&g_audio.vm, &g_audio.patch, mix_scratch, STREAM_BUF_SIZE);
                LeaveCriticalSection(&g_audio.cs);

                /* Master volume (monitor path) - applied before the scope so
                   the display reflects what is actually heard. */
                audio_apply_master_volume(mix_scratch, STREAM_BUF_SIZE);

                audio_write_scope_cache(mix_scratch, STREAM_BUF_SIZE);

                if (g_audio.is_float_format) {
                    memcpy(g_audio.float_buffers[i], mix_scratch, STREAM_BUF_SIZE * 2 * sizeof(float));
                    g_audio.headers[i].dwBufferLength = STREAM_BUF_SIZE * 2 * sizeof(float);
                } else {
                    for (int s = 0; s < STREAM_BUF_SIZE * 2; s++) {
                        float v = mix_scratch[s] * 32767.0f;
                        if (v > 32767.0f) v = 32767.0f;
                        else if (v < -32768.0f) v = -32768.0f;
                        g_audio.pcm16_buffers[i][s] = (short)v;
                    }
                    g_audio.headers[i].dwBufferLength = STREAM_BUF_SIZE * 2 * sizeof(short);
                }

                waveOutPrepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
                waveOutWrite(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
            }
        }
    }
    return 0;
}

static void audio_init(void) {
    if (g_audio.initialized) return;
    memset(&g_audio, 0, sizeof(g_audio));
    g_audio.master_volume = 0.75f;

    InitializeCriticalSection(&g_audio.cs);
    voice_manager_init(&g_audio.vm, HALO_SR);
    /* Patch will be set later via audio_set_patch from UI */

    g_audio.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!g_audio.hEvent) {
        DeleteCriticalSection(&g_audio.cs);
        return;
    }

    WAVEFORMATEX wf = {0};
    wf.wFormatTag = 0x0003; /* WAVE_FORMAT_IEEE_FLOAT */
    wf.nChannels = 2;       /* true stereo signal path */
    wf.nSamplesPerSec = HALO_SR;
    wf.wBitsPerSample = 32;
    wf.nBlockAlign = (wf.nChannels * wf.wBitsPerSample) / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    MMRESULT res = waveOutOpen(&g_audio.hWaveOut, WAVE_MAPPER, &wf, (DWORD_PTR)g_audio.hEvent, 0, CALLBACK_EVENT);
    if (res == MMSYSERR_NOERROR) {
        g_audio.is_float_format = 1;
    } else {
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = (wf.nChannels * wf.wBitsPerSample) / 8;
        wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
        res = waveOutOpen(&g_audio.hWaveOut, WAVE_MAPPER, &wf, (DWORD_PTR)g_audio.hEvent, 0, CALLBACK_EVENT);
        if (res != MMSYSERR_NOERROR) {
            CloseHandle(g_audio.hEvent);
            g_audio.hEvent = NULL;
            DeleteCriticalSection(&g_audio.cs);
            g_audio.init_error = (int)res;   /* surfaced by the UI at startup */
            return;
        }
        g_audio.is_float_format = 0;
    }

    g_audio.running = 1;

    for (int i = 0; i < STREAM_NUM_BUFS; i++) {
        memset(g_audio.float_buffers[i], 0, sizeof(g_audio.float_buffers[i]));
        memset(g_audio.pcm16_buffers[i], 0, sizeof(g_audio.pcm16_buffers[i]));
        memset(&g_audio.headers[i], 0, sizeof(WAVEHDR));

        if (g_audio.is_float_format) {
            g_audio.headers[i].lpData = (LPSTR)g_audio.float_buffers[i];
            g_audio.headers[i].dwBufferLength = STREAM_BUF_SIZE * 2 * sizeof(float);
        } else {
            g_audio.headers[i].lpData = (LPSTR)g_audio.pcm16_buffers[i];
            g_audio.headers[i].dwBufferLength = STREAM_BUF_SIZE * 2 * sizeof(short);
        }

        waveOutPrepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
        waveOutWrite(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
    }

    g_audio.hThread = CreateThread(NULL, 0, audio_thread_proc, NULL, 0, NULL);
    if (g_audio.hThread) {
        SetThreadPriority(g_audio.hThread, THREAD_PRIORITY_TIME_CRITICAL);
        g_audio.initialized = 1;
    } else {
        g_audio.running = 0;
        waveOutReset(g_audio.hWaveOut);
        for (int i = 0; i < STREAM_NUM_BUFS; i++) {
            if (g_audio.headers[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
        }
        waveOutClose(g_audio.hWaveOut);
        g_audio.hWaveOut = NULL;
        CloseHandle(g_audio.hEvent);
        g_audio.hEvent = NULL;
        DeleteCriticalSection(&g_audio.cs);
    }
}

static void audio_set_patch(const HaloPatch* p) {
    if (!p) return;
    EnterCriticalSection(&g_audio.cs);
    g_audio.patch = *p;
    LeaveCriticalSection(&g_audio.cs);
}

static void audio_set_master_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    g_audio.master_volume = vol;
}

static int audio_get_active_voices(void) {
    if (!g_audio.initialized) return 0;
    EnterCriticalSection(&g_audio.cs);
    int n = voice_manager_active_count(&g_audio.vm);
    LeaveCriticalSection(&g_audio.cs);
    return n;
}

static void audio_note_on(int note, float vel) {
    if (!g_audio.initialized) return;
    EnterCriticalSection(&g_audio.cs);
    HaloVoice* v = voice_manager_alloc(&g_audio.vm, note);
    if (v) {
        voice_note_on(v, note, vel, HALO_SR, &g_audio.patch);
    }
    LeaveCriticalSection(&g_audio.cs);
}

static void audio_note_off(int note) {
    if (!g_audio.initialized) return;
    EnterCriticalSection(&g_audio.cs);
    for (int i = 0; i < HALO_MAX_VOICES; i++) {
        if (g_audio.vm.voices[i].active && g_audio.vm.voices[i].note == note) {
            voice_note_off(&g_audio.vm.voices[i]);
        }
    }
    LeaveCriticalSection(&g_audio.cs);
}

static void audio_schedule_reset_clear(DWORD delay_ms) {
    EnterCriticalSection(&g_audio.cs);
    g_audio.reset_kill_tick = GetTickCount() + delay_ms;
    LeaveCriticalSection(&g_audio.cs);
}

static void audio_schedule_audition_release(DWORD delay_ms) {
    EnterCriticalSection(&g_audio.cs);
    g_audio.audition_release_tick = GetTickCount() + delay_ms;
    LeaveCriticalSection(&g_audio.cs);
}

static void audio_all_notes_off(void) {
    if (!g_audio.initialized) return;
    EnterCriticalSection(&g_audio.cs);
    g_audio.reset_kill_tick = 0;
    g_audio.audition_release_tick = 0;
    for (int i = 0; i < HALO_MAX_VOICES; i++) {
        voice_force_idle(&g_audio.vm.voices[i]);
    }
    LeaveCriticalSection(&g_audio.cs);
}

static void audio_get_scope_samples(float* dest, int max_pts) {
    if (!dest || max_pts <= 0) return;
    if (max_pts > SCOPE_RING_SIZE) max_pts = SCOPE_RING_SIZE;

    /* Snapshot the write pointer once: even if the audio thread advances it
       mid-read, this read stays on a consistent ring window instead of
       tearing across the wrap-around. */
    int w_idx = g_audio.scope_write_idx;
    int r_idx = (w_idx - max_pts) & (SCOPE_RING_SIZE - 1);
    for (int i = 0; i < max_pts; i++) {
        dest[i] = g_audio.scope_ring[(r_idx + i) & (SCOPE_RING_SIZE - 1)];
    }
}

static void audio_shutdown(void) {
    if (!g_audio.initialized) return;
    g_audio.running = 0;

    if (g_audio.hEvent) SetEvent(g_audio.hEvent);
    if (g_audio.hThread) {
        WaitForSingleObject(g_audio.hThread, 400);
        CloseHandle(g_audio.hThread);
        g_audio.hThread = NULL;
    }
    if (g_audio.hWaveOut) {
        waveOutReset(g_audio.hWaveOut);
        for (int i = 0; i < STREAM_NUM_BUFS; i++) {
            if (g_audio.headers[i].dwFlags & WHDR_PREPARED) {
                waveOutUnprepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
            }
        }
        waveOutClose(g_audio.hWaveOut);
        g_audio.hWaveOut = NULL;
    }
    if (g_audio.hEvent) {
        CloseHandle(g_audio.hEvent);
        g_audio.hEvent = NULL;
    }
    DeleteCriticalSection(&g_audio.cs);
    g_audio.initialized = 0;
}

#endif /* HALO_AUDIO_H */