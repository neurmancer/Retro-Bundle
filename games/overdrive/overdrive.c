#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <raylib.h>
#include "code/effect.c"
#include "code/audio.c"
#include "code/models.c"
#include "../../physics/doppler.h"

#define WIDTH 1200
#define HEIGHT 900
#define TRAFFIC_COUNT 9
#define ROAD_HALF 8.0f
#define MAX_SPEED (225.0f / 3.6f)

#ifndef FPS
#define FPS 120
#endif

#ifndef VSYNC
#define VSYNC 0
#endif

typedef struct {
    float x;
    float z;
    float speed;
    int color;
    bool passed;
} Traffic;

typedef struct {
    float x;
    float speed;
    float lateralSpeed;
    float distance;
    float scroll;
    float invulnerable;
    float flash;
    int integrity;
    int overtakes;
    bool paused;
    bool over;
    Traffic traffic[TRAFFIC_COUNT];
} Drive;

static const Color SKY = {22, 11, 47, 255};
static const Color ROAD = {24, 21, 48, 255};
static const Color NEON_PINK = {255, 80, 177, 255};
static const Color CYAN = {76, 235, 234, 255};
static const Color NEON_VIOLET = {176, 112, 255, 255};
static const Color PAPER = {242, 235, 255, 255};
static const Color DARK = {11, 9, 27, 255};

float approach(float value, float target, float amount)
{
    if (value < target) { return(fminf(value + amount, target)); }
    return(fmaxf(value - amount, target));
}

void resetDrive(Drive *game)
{
    *game = (Drive){0};
    game->integrity = 3;
    for (int i = 0; i < TRAFFIC_COUNT; i++) {
        game->traffic[i] = (Traffic){(float)((i * 7 + 1) % 3 - 1) * 4.3f, -65.0f - i * 29.0f, 14.0f + (i % 4) * 3.0f, i % TRAFFIC_MODEL_COUNT, false};
    }
}

void updateHandling(Drive *game, float dt, bool throttle, bool brake, int steer)
{
    bool shoulder = fabsf(game->x) > ROAD_HALF - 1.0f;
    if (brake) { game->speed = approach(game->speed, 0.0f, 25.0f * dt); }
    else if (throttle) {
        game->speed += 12.0f * (1.0f - 0.65f * game->speed / MAX_SPEED) * dt;
        game->speed = fminf(game->speed, MAX_SPEED);
    }
    else { game->speed = approach(game->speed, 0.0f, 0.9f * dt); }
    if (shoulder) { game->speed = approach(game->speed, 0.0f, 7.0f * dt); }

    float desiredLateral = steer * fminf(game->speed * 0.16f, 7.0f);
    float grip = shoulder ? 3.2f : 9.0f;
    if (brake) { grip *= 0.7f; }
    game->lateralSpeed = approach(game->lateralSpeed, desiredLateral, grip * dt);
    game->x += game->lateralSpeed * dt;
    float limit = ROAD_HALF + 1.0f;
    if (game->x < -limit) { game->x = -limit; game->lateralSpeed = fmaxf(0.0f, game->lateralSpeed); }
    else if (game->x > limit) { game->x = limit; game->lateralSpeed = fminf(0.0f, game->lateralSpeed); }
}

void updateDrive(Drive *game, float dt)
{
    if (game->paused || game->over) { return; }
    bool throttle = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    bool brake = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    int steer = (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    updateHandling(game, dt, throttle, brake, steer);
    game->distance += game->speed * dt;
    game->scroll = fmodf(game->scroll + game->speed * dt, 16.0f);
    game->invulnerable = fmaxf(0.0f, game->invulnerable - dt);
    game->flash = fmaxf(0.0f, game->flash - dt);
    for (int i = 0; i < TRAFFIC_COUNT; i++) {
        Traffic *car = &game->traffic[i];
        car->z += (game->speed - car->speed) * dt;
        float collisionLength = car->color == CAR_TRUCK ? 5.5f : (car->color == CAR_CLIO ? 3.5f : 3.9f);
        if (!car->passed && car->z > collisionLength) {
            car->passed = true;
            game->overtakes++;
            playNeurosSound(SOUND_SCORE);
        }
        if (game->invulnerable <= 0.0f && fabsf(car->z) < collisionLength && fabsf(car->x - game->x) < (car->color == CAR_CLIO ? 1.65f : 1.8f)) {
            game->integrity--;
            game->speed *= 0.45f;
            game->lateralSpeed *= 0.5f;
            game->invulnerable = 2.0f;
            game->flash = 0.3f;
            playNeurosSound(SOUND_HIT);
            if (game->integrity <= 0) { game->over = true; playNeurosSound(SOUND_LOSE); }
        }
        if (car->z < -280.0f) {
            car->z = -170.0f;
            car->x = (float)(rand() % 3 - 1) * 4.3f;
            for (int j = 0; j < TRAFFIC_COUNT; j++) {
                if (j != i && fabsf(game->traffic[j].x - car->x) < 1.0f && fabsf(game->traffic[j].z - car->z) < 12.0f) {
                    car->z -= 15.0f;
                    j = -1;
                }
            }
            car->passed = false;
        }
        if (car->z > 20.0f) {
            float farthest = -180.0f;
            for (int j = 0; j < TRAFFIC_COUNT; j++) {
                if (j != i && game->traffic[j].z < farthest) { farthest = game->traffic[j].z; }
            }
            car->z = farthest - 26.0f - (float)(rand() % 15);
            car->x = (float)(rand() % 3 - 1) * 4.3f;
            car->speed = 12.0f + (float)(rand() % 13);
            car->color = rand() % TRAFFIC_MODEL_COUNT;
            car->passed = false;
        }
    }
}

typedef struct {
    Sound engine;
    bool ready;
    float pitch;
    float volume;
} TrafficAudio;

float trafficDopplerPitch(const Drive *game, const Traffic *traffic)
{
    DopplerState source = {{traffic->x, 0, traffic->z}, {0, 0, -traffic->speed}};
    DopplerState observer = {{game->x, 0, 0}, {game->lateralSpeed, 0, -game->speed}};
    DopplerVec3 direction = {observer.position.x - source.position.x, 0, -source.position.z};
    double factor = 1.0;
    double wavelength = 1.0;
    if (dopplerShift(DOPPLER_SOUND, 343.0, source, observer, direction, &factor, &wavelength) != DOPPLER_OK) { return(1.0f); }
    return((float)fmax(0.65, fmin(1.5, factor)));
}

TrafficAudio initTrafficAudio(void)
{
    TrafficAudio audio = {0};
    audio.pitch = 1.0f;
    if (!neurosAudio.ready) { return(audio); }
    unsigned int count = NEUROS_SAMPLE_RATE;
    int16_t *samples = MemAlloc(count * sizeof(*samples));
    if (samples == NULL) { return(audio); }
    for (unsigned int i = 0; i < count; i++) {
        float phase = 2.0f * PI * 90.0f * i / NEUROS_SAMPLE_RATE;
        float tone = 0.50f * sinf(phase) + 0.25f * sinf(phase * 2.0f) + 0.12f * sinf(phase * 3.0f);
        samples[i] = (int16_t)(tone * 6500.0f);
    }
    Wave wave = {count, NEUROS_SAMPLE_RATE, 16, 1, samples};
    audio.engine = LoadSoundFromWave(wave);
    UnloadWave(wave);
    audio.ready = audio.engine.stream.buffer != NULL;
    return(audio);
}

void updateTrafficAudio(TrafficAudio *audio, const Drive *game, float dt, bool active)
{
    if (!audio->ready) { return; }
    if (!active || neurosAudio.muted || game->paused || game->over) {
        if (IsSoundPlaying(audio->engine)) { StopSound(audio->engine); }
        audio->volume = 0.0f;
        return;
    }
    const Traffic *nearest = NULL;
    float distance = 90.0f;
    for (int i = 0; i < TRAFFIC_COUNT; i++) {
        const Traffic *car = &game->traffic[i];
        float current = hypotf(car->x - game->x, car->z);
        if (current < distance) { distance = current; nearest = car; }
    }
    float targetPitch = nearest ? trafficDopplerPitch(game, nearest) : 1.0f;
    float targetVolume = nearest ? 0.30f * powf(fmaxf(0.0f, 1.0f - distance / 90.0f), 2.0f) : 0.0f;
    float blend = 1.0f - expf(-dt * 6.0f);
    audio->pitch += (targetPitch - audio->pitch) * blend;
    audio->volume += (targetVolume - audio->volume) * blend;
    SetSoundPitch(audio->engine, audio->pitch);
    SetSoundVolume(audio->engine, audio->volume);
    if (!IsSoundPlaying(audio->engine)) { PlaySound(audio->engine); }
}

void unloadTrafficAudio(TrafficAudio *audio)
{
    if (audio->ready) { UnloadSound(audio->engine); audio->ready = false; }
}

void drawSunset(void)
{
    ClearBackground(SKY);
    DrawRectangleGradientV(0, 0, WIDTH, 550, (Color){17, 10, 43, 255}, (Color){119, 38, 103, 255});
    for (int i = 0; i < 60; i++) {
        int x = (i * 197 + 31) % WIDTH;
        int y = (i * 83 + 37) % 330;
        DrawRectangle(x, y, i % 7 == 0 ? 3 : 2, i % 7 == 0 ? 3 : 2, Fade(PAPER, 0.65f));
    }
#if RAYLIB_VERSION_MAJOR >= 6
    DrawCircleGradient((Vector2){600, 315}, 150.0f, (Color){255, 165, 129, 255}, (Color){255, 76, 157, 255});
#else
    DrawCircleGradient(600, 315, 150.0f, (Color){255, 165, 129, 255}, (Color){255, 76, 157, 255});
#endif
    for (int i = 0; i < 7; i++) {
        int y = 330 + i * 18;
        int half = (int)sqrtf(fmaxf(0.0f, 150.0f * 150.0f - (float)((y - 315) * (y - 315))));
        DrawRectangle(600 - half, y, half * 2, 3 + i, (Color){91, 30, 79, 255});
    }
    DrawTriangle((Vector2){0, 500}, (Vector2){0, 390}, (Vector2){260, 500}, (Color){31, 19, 66, 255});
    DrawTriangle((Vector2){110, 500}, (Vector2){310, 355}, (Vector2){510, 500}, (Color){31, 19, 66, 255});
    DrawTriangle((Vector2){690, 500}, (Vector2){890, 365}, (Vector2){1100, 500}, (Color){31, 19, 66, 255});
    DrawTriangle((Vector2){950, 500}, (Vector2){1200, 385}, (Vector2){1200, 500}, (Color){31, 19, 66, 255});
}

void drawLampPool(float x, float z)
{
    const int segments = 24;
    const float radius = 5.2f;
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < segments; i++) {
        float a = 2.0f * PI * i / segments;
        float b = 2.0f * PI * (i + 1) / segments;
        rlColor4ub(65, 222, 246, 54);
        rlVertex3f(x, 0.006f, z);
        rlColor4ub(65, 222, 246, 0);
        rlVertex3f(x + cosf(a) * radius, 0.006f, z + sinf(a) * radius);
        rlVertex3f(x + cosf(b) * radius, 0.006f, z + sinf(b) * radius);
    }
    rlEnd();
}

void drawWorld(const Drive *game, const DriveModels *models)
{
    Camera3D camera = {0};
    camera.position = (Vector3){game->x * 0.35f, 5.1f, 12.5f};
    camera.target = (Vector3){game->x * 0.18f, 0.6f, -30.0f};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 52.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    if (models->lightingReady) {
        int scrollLocation = GetShaderLocation(models->lighting, "roadScroll");
        if (scrollLocation >= 0) { SetShaderValue(models->lighting, scrollLocation, &game->scroll, SHADER_UNIFORM_FLOAT); }
    }
    BeginMode3D(camera);
    DrawPlane((Vector3){0, -0.10f, -115}, (Vector2){500, 300}, (Color){17, 12, 38, 255});
    DrawCube((Vector3){0, -0.06f, -115}, ROAD_HALF * 2, 0.08f, 270, ROAD);
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    for (int i = 0; i < 20; i++) {
        float z = -16.0f * i + game->scroll;
        drawLampPool(-10.43f, z);
        drawLampPool(10.43f, z);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    for (int i = 0; i < 20; i++) {
        float z = -16.0f * i + game->scroll;
        DrawCube((Vector3){-ROAD_HALF, 0.02f, z}, 0.16f, 0.06f, 16.0f, NEON_PINK);
        DrawCube((Vector3){ROAD_HALF, 0.02f, z}, 0.16f, 0.06f, 16.0f, NEON_PINK);
        for (int lane = -1; lane <= 1; lane += 2) {
            DrawCube((Vector3){lane * 2.67f, 0.02f, z}, 0.10f, 0.04f, 7.0f, Fade(PAPER, 0.8f));
        }
        for (int side = -1; side <= 1; side += 2) {
            if (models->streetlightLoaded) {
                DrawModelEx(models->streetlight, (Vector3){side * 11.0f, 0, z}, (Vector3){0, 1, 0}, side > 0 ? 90.0f : -90.0f, (Vector3){1, 1, 1}, WHITE);
            }
            DrawCube((Vector3){side * 29.0f, -0.01f, z}, 36.0f, 0.025f, 0.08f, Fade(NEON_VIOLET, 0.38f));
        }
    }
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 7; i++) {
            DrawCube((Vector3){side * (11.0f + i * 6.0f), -0.01f, -115}, 0.06f, 0.025f, 270, Fade(NEON_VIOLET, 0.3f));
        }
    }
    for (int i = 0; i < TRAFFIC_COUNT; i++) {
        const Traffic *car = &game->traffic[i];
        if (car->z < 12.0f && car->z > -270.0f) { drawCarModel(models, (CarModel)car->color, car->x, car->z); }
    }
    if (game->invulnerable <= 0.0f || (int)(game->invulnerable * 8.0f) % 2 == 0) { drawCarModel(models, CAR_SENNA, game->x, 0); }
    EndMode3D();
}

void drawCentered(const char *text, int y, int size, Color color)
{
    DrawText(text, (WIDTH - MeasureText(text, size)) / 2, y, size, color);
}

void drawDrive(const Drive *game, const DriveModels *models, const NeurosEffect *effect, bool title)
{
    drawSunset();
    drawWorld(game, models);
    if (effect->target.id != 0 && effect->targetScale > 1) {
        Camera2D camera = {0};
        camera.zoom = (float)effect->targetScale;
        BeginMode2D(camera);
    }
    DrawRectangle(0, 0, WIDTH, 68, Fade(DARK, 0.85f));
    DrawText("OVERDRIVE", 30, 21, 25, NEON_PINK);
    DrawText(TextFormat("DIST  %07.1f KM", game->distance / 1000.0f), 320, 24, 20, PAPER);
    DrawText(TextFormat("PASSED  %03d", game->overtakes), 615, 24, 20, CYAN);
    DrawText(TextFormat("HULL  %d / 3", game->integrity), 940, 24, 20, game->integrity == 1 ? NEON_PINK : PAPER);
    if (fabsf(game->x) > ROAD_HALF - 1.0f) { DrawText("LOW GRIP", 1000, 100, 20, NEON_PINK); }
    DrawRectangle(30, 750, 240, 100, Fade(DARK, 0.85f));
    DrawText(TextFormat("%03d", (int)lroundf(game->speed * 3.6f)), 45, 758, 52, PAPER);
    DrawText("KM/H", 160, 790, 20, CYAN);
    DrawRectangle(45, 825, 200, 7, Fade(PAPER, 0.2f));
    DrawRectangle(45, 825, (int)(200.0f * game->speed / MAX_SPEED), 7, NEON_PINK);
    DrawText("W / UP ACCELERATE   S / DOWN BRAKE   A D / ARROWS STEER   P PAUSE   F FX   M SOUND   F11 FULLSCREEN", 285, 860, 13, PAPER);
    if (neurosAudio.muted || !neurosAudio.ready) { DrawText("SOUND OFF", 1060, 72, 14, PAPER); }
    if (game->flash > 0.0f) { DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(NEON_PINK, game->flash * 0.5f)); }
    if (title || game->paused || game->over) {
        DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(DARK, 0.74f));
        if (title) {
            drawCentered("OVERDRIVE", 250, 82, NEON_PINK);
            drawCentered("ENDLESS HIGHWAY  /  MIDNIGHT RUN", 350, 22, CYAN);
            drawCentered("STEER THROUGH TRAFFIC. KEEP YOUR HULL INTACT.", 440, 20, PAPER);
            drawCentered("W / UP ACCELERATE    S / DOWN BRAKE", 495, 20, PAPER);
            drawCentered("A D / LEFT RIGHT STEER", 525, 20, PAPER);
            drawCentered("PRESS ENTER TO DRIVE", 605, 28, CYAN);
        }
        else if (game->paused) {
            drawCentered("PAUSED", 360, 64, CYAN);
            drawCentered("PRESS P TO RESUME", 445, 22, PAPER);
        }
        else {
            drawCentered("END OF THE ROAD", 330, 60, NEON_PINK);
            drawCentered(TextFormat("%.1f KM  //  %d CARS PASSED", game->distance / 1000.0f, game->overtakes), 420, 24, PAPER);
            drawCentered("PRESS R TO DRIVE AGAIN", 480, 22, CYAN);
        }
    }
}

int main(void)
{
    srand((unsigned int)time(NULL));
    if (VSYNC) { SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT); }
    else { SetConfigFlags(FLAG_MSAA_4X_HINT); }
    InitWindow(WIDTH, HEIGHT, "NeurOS | Overdrive");
    if (!IsWindowReady()) { fprintf(stderr, "Could not open the game window.\n"); return(-1); }
    if (!VSYNC) { SetTargetFPS((FPS > 0 && FPS <= 300) ? FPS : 120); }
    DriveModels models;
    if (!loadDriveModels(&models)) {
        unloadDriveModels(&models);
        CloseWindow();
        return(-1);
    }
    initNeurosAudio();
    TrafficAudio trafficAudio = initTrafficAudio();
    NeurosEffect effect = loadNeurosEffectNamed(WIDTH, HEIGHT, 0.7f, (Vector3){1.0f, 0.30f, 0.68f}, "overdrive.fs");
    setNeurosRenderScale(&effect, 2);
    Drive game;
    resetDrive(&game);
    bool title = true;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) { toggleNeurosFullscreen(&effect); }
        if (IsKeyPressed(KEY_F) && effect.ready) { effect.enabled = !effect.enabled; }
        if (IsKeyPressed(KEY_M)) { toggleNeurosAudio(); }
        if (title && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) { title = false; }
        else if (!title && game.over && IsKeyPressed(KEY_R)) { resetDrive(&game); }
        else if (!title && !game.over && IsKeyPressed(KEY_P)) { game.paused = !game.paused; }
        float dt = fminf(GetFrameTime(), 0.05f);
        if (!title) { updateDrive(&game, dt); }
        updateTrafficAudio(&trafficAudio, &game, dt, !title);
        beginNeurosEffect(&effect);
        drawDrive(&game, &models, &effect, title);
        endNeurosEffect(&effect);
    }
    unloadTrafficAudio(&trafficAudio);
    unloadNeurosAudio();
    unloadDriveModels(&models);
    unloadNeurosEffect(&effect);
    CloseWindow();
    return(0);
}
