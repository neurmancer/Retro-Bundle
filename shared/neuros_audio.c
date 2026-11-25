#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <raylib.h>

#define NEUROS_SAMPLE_RATE 22050

typedef enum {
    SOUND_MOVE,
    SOUND_ROTATE,
    SOUND_HIT,
    SOUND_SCORE,
    SOUND_CLEAR,
    SOUND_LOSE,
    SOUND_WIN,
    SOUND_COUNT
} NeurosSound;

typedef struct {
    Sound sounds[SOUND_COUNT];
    bool ready;
    bool muted;
} NeurosAudio;

static NeurosAudio neurosAudio = {0};

void initNeurosAudio(void)
{
    static const float frequencies[SOUND_COUNT][3] = {
        {440.0f, 330.0f, 0.045f}, {510.0f, 760.0f, 0.075f},
        {310.0f, 190.0f, 0.075f}, {560.0f, 850.0f, 0.14f},
        {500.0f, 1050.0f, 0.23f}, {270.0f, 90.0f, 0.38f},
        {450.0f, 980.0f, 0.38f}
    };
    InitAudioDevice();
    if (!IsAudioDeviceReady()) { return; }
    neurosAudio.ready = true;
    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        float duration = frequencies[sound][2];
        unsigned int count = (unsigned int)(NEUROS_SAMPLE_RATE * duration);
        int16_t *samples = MemAlloc(count * sizeof(*samples));
        if (samples == NULL) { continue; }
        float phase = 0.0f;
        for (unsigned int i = 0; i < count; i++) {
            float t = (float)i / NEUROS_SAMPLE_RATE;
            float progress = t / duration;
            float frequency = frequencies[sound][0] + (frequencies[sound][1] - frequencies[sound][0]) * progress;
            phase += 2.0f * PI * frequency / NEUROS_SAMPLE_RATE;
            float envelope = fminf(t / 0.008f, 1.0f) * (1.0f - progress) * (1.0f - progress);
            float tone = 0.78f * sinf(phase) + 0.22f * sinf(phase * 2.0f);
            samples[i] = (int16_t)(tone * envelope * 9000.0f);
        }
        Wave wave = {count, NEUROS_SAMPLE_RATE, 16, 1, samples};
        neurosAudio.sounds[sound] = LoadSoundFromWave(wave);
        UnloadWave(wave);
    }
}

void playNeurosSound(NeurosSound sound)
{
    if (neurosAudio.ready && !neurosAudio.muted && neurosAudio.sounds[sound].stream.buffer != NULL) {
        PlaySound(neurosAudio.sounds[sound]);
    }
}

void toggleNeurosAudio(void)
{
    neurosAudio.muted = !neurosAudio.muted;
    if (neurosAudio.ready && neurosAudio.muted) {
        for (int i = 0; i < SOUND_COUNT; i++) {
            if (neurosAudio.sounds[i].stream.buffer != NULL) { StopSound(neurosAudio.sounds[i]); }
        }
    }
}

void unloadNeurosAudio(void)
{
    if (!neurosAudio.ready) { return; }
    for (int i = 0; i < SOUND_COUNT; i++) {
        if (neurosAudio.sounds[i].stream.buffer != NULL) { UnloadSound(neurosAudio.sounds[i]); }
    }
    CloseAudioDevice();
    neurosAudio = (NeurosAudio){0};
}
