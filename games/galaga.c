#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <raylib.h>
#include <rlgl.h>
#include "../shared/neuros_effect.c"
#include "../shared/neuros_start.c"
#include "../shared/neuros_audio.c"
#include "../physics/doppler.h"

#define WIDTH 1200
#define HEIGHT 900
#define ROWS 4
#define COLUMNS 9
#define ALIEN_COUNT (ROWS * COLUMNS)
#define SHOT_COUNT 128
#define PARTICLE_COUNT 420
#define PLAYER_Y 790.0f
#define PLAYER_SPEED 430.0f
#define GAME_SOUND_SPEED 343.0
#define METERS_PER_PIXEL 0.1
#define COLOR_SPEED_SCALE (DOPPLER_LIGHT_SPEED / 6000.0)

#ifndef FPS
#define FPS 120
#endif

#ifndef VSYNC
#define VSYNC 0
#endif

typedef struct {
    Vector2 position;
    Vector2 velocity;
    bool alive;
    bool diving;
    int health;
    float diveTime;
    float diveOriginX;
    float diveOriginY;
    float diveTarget;
    int row;
    int column;
} Alien;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    bool active;
    bool friendly;
    float age;
    float speed;
    float acceleration;
} Shot;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float life;
    float lifetime;
    float size;
} Particle;

typedef struct {
    Alien aliens[ALIEN_COUNT];
    Shot shots[SHOT_COUNT];
    Particle particles[PARTICLE_COUNT];
    Vector2 player;
    Vector2 playerVelocity;
    float formationTime;
    float diveTimer;
    float enemyShotTimer;
    float fireTimer;
    float invulnerability;
    float shake;
    float elapsed;
    float waveNotice;
    int lives;
    int score;
    int wave;
    int remaining;
    bool paused;
    bool gameOver;
} Game;

static const Color BACKGROUND = {8, 11, 22, 255};
static const Color MINT = {52, 236, 168, 255};
static const Color NEON_VIOLET = {169, 92, 252, 255};
static const Color NEON_BLUE = {53, 191, 255, 255};
static const Color REDSHIFT = {255, 93, 114, 255};
static const Color TEXT = {229, 241, 245, 255};
static Shader lightShader = {0};
static Texture2D lightTexture = {0};
static bool lightReady = false;
static bool lightEnabled = true;
static Sound projectileSources[2] = {0};
static Sound projectileVoices[2][SHOT_COUNT] = {0};

static const Color MUTED = {154, 175, 192, 255};

void resetGame(Game *game);
void startWave(Game *game);
void updateGame(Game *game, float dt);
void updatePlayer(Game *game, float dt, int horizontal, int vertical);
void updateAliens(Game *game, float dt);
void updateShots(Game *game, float dt);
void launchShot(Game *game, Vector2 position, Vector2 velocity, bool friendly, Vector2 sourceVelocity);
void loseLife(Game *game);
void drawGame(const Game *game);
void emitParticles(Game *game, Vector2 position, Color color, int count, float force);
void loadLight(void);
void drawLight(Vector2 position, float radius, Color color);
void drawPixelSprite(const char *const *sprite, int rows, int columns, Vector2 position, int scale, Color color);
Color shiftColor(Vector2 position, Vector2 velocity, Color base);
float soundPitch(Vector2 source, Vector2 velocity, Vector2 listener, Vector2 listenerVelocity);
void playShotSound(NeurosSound sound, float pitch);
void initProjectileAudio(void);
void updateProjectileAudio(const Game *game);
void stopProjectileVoice(int index);
void unloadProjectileAudio(void);

int main(void)
{
    srand((unsigned int)time(NULL));
    if (VSYNC) { SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT); }
    else { SetConfigFlags(FLAG_MSAA_4X_HINT); }
    InitWindow(WIDTH, HEIGHT, "NeurOS | Galaga");
    if (!IsWindowReady()) { fprintf(stderr, "Could not open the game window.\n"); return(-1); }
    if (!VSYNC) { SetTargetFPS((FPS > 0 && FPS <= 300) ? FPS : 120); }
    initNeurosAudio();
    initProjectileAudio();
    loadLight();
    NeurosEffect effect = loadNeurosEffectNamed(WIDTH, HEIGHT, 0.85f, (Vector3){0.66f, 0.36f, 0.99f}, "galaga_glow.fs");
    Game game = {0};
    resetGame(&game);
    bool showTitle = true;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) { toggleNeurosFullscreen(&effect); }
        if (IsKeyPressed(KEY_M)) { toggleNeurosAudio(); }
        if (IsKeyPressed(KEY_F)) {
            if (effect.ready) { effect.enabled = !effect.enabled; }
            lightEnabled = !lightEnabled;
        }
        if (showTitle) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { showTitle = false; }
            beginNeurosEffect(&effect);
            drawArcadeStart(ARCADE_GALAGA);
            endNeurosEffect(&effect);
            continue;
        }
        if (game.gameOver && IsKeyPressed(KEY_R)) { resetGame(&game); }
        else if (!game.gameOver && IsKeyPressed(KEY_P)) { game.paused = !game.paused; }
        if (!game.paused && !game.gameOver) {
            float dt = GetFrameTime();
            if (dt > 0.033f) { dt = 0.033f; }
            updateGame(&game, dt);
        }
        updateProjectileAudio(&game);
        beginNeurosEffect(&effect);
        drawGame(&game);
        endNeurosEffect(&effect);
    }
    if (lightTexture.id != 0) { UnloadTexture(lightTexture); }
    if (lightReady) { UnloadShader(lightShader); }
    unloadProjectileAudio();
    unloadNeurosAudio();
    unloadNeurosEffect(&effect);
    CloseWindow();
    return(0);
}

void resetGame(Game *game)
{
    *game = (Game){0};
    game->player = (Vector2){WIDTH / 2.0f, PLAYER_Y};
    game->lives = 3;
    game->wave = 1;
    game->invulnerability = 1.5f;
    startWave(game);
}

void startWave(Game *game)
{
    game->formationTime = 0.0f;
    game->diveTimer = 1.5f;
    game->enemyShotTimer = 1.0f;
    game->remaining = ALIEN_COUNT;
    game->waveNotice = 2.0f;
    for (int i = 0; i < SHOT_COUNT; i++) { game->shots[i].active = false; }
    for (int i = 0; i < ALIEN_COUNT; i++) {
        int row = i / COLUMNS;
        int column = i % COLUMNS;
        game->aliens[i] = (Alien){.position = {280.0f + 80.0f * column, 155.0f + 55.0f * row}, .alive = true, .health = row == 0 ? 2 : 1, .row = row, .column = column};
    }
}

void updateGame(Game *game, float dt)
{
    int horizontal = (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
    int vertical = (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) - (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W));
    updatePlayer(game, dt, horizontal, vertical);
    game->elapsed += dt;
    game->waveNotice -= dt;
    game->shake = fmaxf(0.0f, game->shake - dt * 3.5f);
    game->fireTimer -= dt;
    if (game->invulnerability > 0.0f) { game->invulnerability -= dt; }
    if (IsKeyDown(KEY_SPACE) && game->fireTimer <= 0.0f) {
        launchShot(game, (Vector2){game->player.x, game->player.y - 25.0f}, (Vector2){0, -560.0f}, true, game->playerVelocity);
        game->fireTimer = 0.20f;
    }
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        Particle *particle = &game->particles[i];
        if (particle->life <= 0.0f) { continue; }
        particle->life -= dt;
        particle->position.x += particle->velocity.x * dt;
        particle->position.y += particle->velocity.y * dt;
        particle->velocity.x *= fmaxf(0.0f, 1.0f - 2.0f * dt);
        particle->velocity.y *= fmaxf(0.0f, 1.0f - 2.0f * dt);
    }
    updateAliens(game, dt);
    if (game->gameOver) { return; }
    updateShots(game, dt);
    if (!game->gameOver && game->remaining == 0) {
        game->wave++;
        playNeurosSound(SOUND_CLEAR);
        startWave(game);
    }
}

void updatePlayer(Game *game, float dt, int horizontal, int vertical)
{
    Vector2 input = {(float)horizontal, (float)vertical};
    float length = hypotf(input.x, input.y);
    if (length > 0.0f) { input.x /= length; input.y /= length; }
    game->playerVelocity = (Vector2){input.x * PLAYER_SPEED, input.y * PLAYER_SPEED};
    game->player.x += game->playerVelocity.x * dt;
    game->player.y += game->playerVelocity.y * dt;
    if (game->player.x < 35.0f || game->player.x > WIDTH - 35.0f) { game->playerVelocity.x = 0.0f; }
    if (game->player.y < 355.0f || game->player.y > 805.0f) { game->playerVelocity.y = 0.0f; }
    game->player.x = fmaxf(35.0f, fminf(WIDTH - 35.0f, game->player.x));
    game->player.y = fmaxf(355.0f, fminf(805.0f, game->player.y));
}

void updateAliens(Game *game, float dt)
{
    game->formationTime += dt;
    game->diveTimer -= dt;
    game->enemyShotTimer -= dt;
    if (game->diveTimer <= 0.0f && game->remaining > 0) {
        int candidates[ALIEN_COUNT];
        int count = 0;
        for (int i = 0; i < ALIEN_COUNT; i++) {
            if (game->aliens[i].alive && !game->aliens[i].diving) { candidates[count++] = i; }
        }
        if (count > 0) {
            Alien *alien = &game->aliens[candidates[rand() % count]];
            alien->diving = true;
            alien->diveTime = 0.0f;
            alien->diveOriginX = alien->position.x;
            alien->diveOriginY = alien->position.y;
            alien->diveTarget = game->player.x;
        }
        game->diveTimer = fmaxf(0.55f, 2.0f - 0.10f * game->wave);
    }
    if (game->enemyShotTimer <= 0.0f && game->remaining > 0) {
        int candidates[ALIEN_COUNT];
        int count = 0;
        for (int i = 0; i < ALIEN_COUNT; i++) {
            if (game->aliens[i].alive) { candidates[count++] = i; }
        }
        if (count > 0) {
            Alien *alien = &game->aliens[candidates[rand() % count]];
            Vector2 aim = {game->player.x - alien->position.x, game->player.y - alien->position.y};
            float length = hypotf(aim.x, aim.y);
            if (length > 0.0f) {
                float speed = fminf(420.0f, 220.0f + 12.0f * game->wave);
                launchShot(game, alien->position, (Vector2){aim.x * speed / length, aim.y * speed / length}, false, alien->velocity);
                if (game->wave >= 3 && alien->diving) {
                    float vx = aim.x * speed / length;
                    float vy = aim.y * speed / length;
                    launchShot(game, alien->position, (Vector2){vx * 0.97f - vy * 0.24f, vx * 0.24f + vy * 0.97f}, false, alien->velocity);
                    launchShot(game, alien->position, (Vector2){vx * 0.97f + vy * 0.24f, -vx * 0.24f + vy * 0.97f}, false, alien->velocity);
                }
            }
        }
        game->enemyShotTimer = fmaxf(0.35f, 1.4f - game->wave * 0.06f);
    }
    for (int i = 0; i < ALIEN_COUNT; i++) {
        Alien *alien = &game->aliens[i];
        if (!alien->alive) { continue; }
        Vector2 old = alien->position;
        float homeX = 280.0f + 80.0f * alien->column + sinf(game->formationTime * 0.9f) * 90.0f;
        float homeY = 155.0f + 55.0f * alien->row + sinf(game->formationTime * 1.3f) * 12.0f;
        if (alien->diving) {
            alien->diveTime += dt * fminf(1.5f, 0.75f + game->wave * 0.04f);
            float t = alien->diveTime;
            if (t < 1.0f) {
                float curve = t * t * (3.0f - 2.0f * t);
                alien->position.x = alien->diveOriginX + (alien->diveTarget - alien->diveOriginX) * curve + sinf(t * PI * 2.0f) * 80.0f;
                alien->position.y = alien->diveOriginY + (HEIGHT + 80.0f - alien->diveOriginY) * t * t;
            }
            else {
                alien->diving = false;
                alien->position = (Vector2){homeX, homeY};
                old = alien->position;
            }
        }
        else { alien->position = (Vector2){homeX, homeY}; }
        alien->velocity = dt > 0.0f ? (Vector2){(alien->position.x - old.x) / dt, (alien->position.y - old.y) / dt} : (Vector2){0};
        if (game->invulnerability <= 0.0f && CheckCollisionCircles(alien->position, 22.0f, game->player, 20.0f)) {
            alien->alive = false;
            game->remaining--;
            emitParticles(game, alien->position, NEON_VIOLET, 18, 240.0f);
            loseLife(game);
            if (game->gameOver) { return; }
        }
    }
}

void updateShots(Game *game, float dt)
{
    for (int i = 0; i < SHOT_COUNT; i++) {
        Shot *shot = &game->shots[i];
        if (!shot->active) { continue; }
        shot->age += dt;
        float currentSpeed = hypotf(shot->velocity.x, shot->velocity.y);
        if (currentSpeed > 0.0f) {
            float acceleration = !shot->friendly && shot->age > 0.65f ? 260.0f : shot->acceleration;
            float nextSpeed = fmaxf(120.0f, fminf(950.0f, currentSpeed + acceleration * dt));
            shot->velocity.x *= nextSpeed / currentSpeed;
            shot->velocity.y *= nextSpeed / currentSpeed;
            shot->speed = nextSpeed;
        }
        shot->position.x += shot->velocity.x * dt;
        shot->position.y += shot->velocity.y * dt;
        if (shot->position.y < 80.0f || shot->position.y > HEIGHT || shot->position.x < 0 || shot->position.x > WIDTH) { shot->active = false; continue; }
        if (shot->friendly) {
            for (int j = 0; j < ALIEN_COUNT; j++) {
                Alien *alien = &game->aliens[j];
                if (alien->alive && CheckCollisionCircles(shot->position, 6.0f, alien->position, 21.0f)) {
                    shot->active = false;
                    alien->health--;
                    game->shake = fminf(1.0f, game->shake + 0.16f);
                    if (alien->health <= 0) {
                        alien->alive = false;
                        game->remaining--;
                        game->score += alien->diving ? 200 : 100;
                        emitParticles(game, alien->position, shiftColor(alien->position, alien->velocity, NEON_VIOLET), 16, 220.0f);
                    }
                    else { emitParticles(game, alien->position, TEXT, 6, 110.0f); }
                    playNeurosSound(SOUND_HIT);
                    break;
                }
            }
        }
        else if (game->invulnerability <= 0.0f && CheckCollisionCircles(shot->position, 6.0f, game->player, 19.0f)) {
            shot->active = false;
            loseLife(game);
            if (game->gameOver) { return; }
        }
    }
}

void loseLife(Game *game)
{
    game->lives--;
    game->shake = 1.0f;
    emitParticles(game, game->player, MINT, 38, 330.0f);
    game->invulnerability = 2.0f;
    if (game->lives <= 0) { game->gameOver = true; }
    playNeurosSound(SOUND_LOSE);
}

float soundPitch(Vector2 source, Vector2 velocity, Vector2 listener, Vector2 listenerVelocity)
{
    DopplerState emitter = {{source.x * METERS_PER_PIXEL, source.y * METERS_PER_PIXEL, 0}, {velocity.x * METERS_PER_PIXEL, velocity.y * METERS_PER_PIXEL, 0}};
    DopplerState receiver = {{listener.x * METERS_PER_PIXEL, listener.y * METERS_PER_PIXEL, 0}, {listenerVelocity.x * METERS_PER_PIXEL, listenerVelocity.y * METERS_PER_PIXEL, 0}};
    DopplerVec3 direction = {receiver.position.x - emitter.position.x, receiver.position.y - emitter.position.y, 0};
    double frequency = 1.0;
    double wavelength = 1.0;
    if (dopplerShift(DOPPLER_SOUND, GAME_SOUND_SPEED, emitter, receiver, direction, &frequency, &wavelength) != DOPPLER_OK) { return(1.0f); }
    return((float)fmax(0.5, fmin(2.0, frequency)));
}

void playShotSound(NeurosSound sound, float pitch)
{
    if (!neurosAudio.ready || neurosAudio.muted || neurosAudio.sounds[sound].stream.buffer == NULL) { return; }
    SetSoundPitch(neurosAudio.sounds[sound], pitch);
    PlaySound(neurosAudio.sounds[sound]);
}

void launchShot(Game *game, Vector2 position, Vector2 velocity, bool friendly, Vector2 sourceVelocity)
{
    for (int i = 0; i < SHOT_COUNT; i++) {
        if (game->shots[i].active) { continue; }
        stopProjectileVoice(i);
        game->shots[i] = (Shot){.position = position, .velocity = velocity, .active = true, .friendly = friendly, .speed = hypotf(velocity.x, velocity.y), .acceleration = friendly ? 800.0f : -130.0f};
        playShotSound(friendly ? SOUND_ROTATE : SOUND_SCORE, soundPitch(position, sourceVelocity, game->player, game->playerVelocity));
        return;
    }
}

void initProjectileAudio(void)
{
    if (!neurosAudio.ready) { return; }
    const unsigned int sampleCount = NEUROS_SAMPLE_RATE * 4;
    for (int kind = 0; kind < 2; kind++) {
        int16_t *samples = MemAlloc(sampleCount * sizeof(*samples));
        if (samples == NULL) { continue; }
        float frequency = kind == 0 ? 430.0f : 680.0f;
        for (unsigned int i = 0; i < sampleCount; i++) {
            float t = (float)i / NEUROS_SAMPLE_RATE;
            float envelope = fminf(1.0f, fminf(t / 0.012f, (4.0f - t) / 0.012f));
            float phase = 2.0f * PI * frequency * t;
            float pulse = 0.75f + 0.25f * sinf(2.0f * PI * 7.0f * t);
            float tone = 0.8f * sinf(phase) + 0.2f * sinf(3.0f * phase);
            samples[i] = (int16_t)(tone * pulse * envelope * 11000.0f);
        }
        Wave wave = {sampleCount, NEUROS_SAMPLE_RATE, 16, 1, samples};
        projectileSources[kind] = LoadSoundFromWave(wave);
        UnloadWave(wave);
    }
}

void stopProjectileVoice(int index)
{
    if (!neurosAudio.ready) { return; }
    for (int kind = 0; kind < 2; kind++) {
        Sound voice = projectileVoices[kind][index];
        if (voice.stream.buffer != NULL && IsSoundPlaying(voice)) { StopSound(voice); }
    }
}

void updateProjectileAudio(const Game *game)
{
    if (!neurosAudio.ready) { return; }
    int active = 0;
    for (int i = 0; i < SHOT_COUNT; i++) {
        if (game->shots[i].active) { active++; }
    }
    float mix = 0.32f / sqrtf((float)(active > 0 ? active : 1));
    for (int i = 0; i < SHOT_COUNT; i++) {
        const Shot *shot = &game->shots[i];
        if (!shot->active || game->paused || game->gameOver || neurosAudio.muted) {
            stopProjectileVoice(i);
            continue;
        }
        int kind = shot->friendly ? 1 : 0;
        if (projectileSources[kind].stream.buffer == NULL) { continue; }
        Sound *voice = &projectileVoices[kind][i];
        if (voice->stream.buffer == NULL) { *voice = LoadSoundAlias(projectileSources[kind]); }
        if (voice->stream.buffer == NULL) { continue; }
        float dx = shot->position.x - game->player.x;
        float dy = shot->position.y - game->player.y;
        float distance = hypotf(dx, dy);
        float proximity = 0.18f + 0.82f / (1.0f + distance / 200.0f);
        float pan = fmaxf(-1.0f, fminf(1.0f, dx / 500.0f));
        SetSoundPitch(*voice, soundPitch(shot->position, shot->velocity, game->player, game->playerVelocity));
        SetSoundVolume(*voice, mix * proximity);
        SetSoundPan(*voice, pan);
        if (!IsSoundPlaying(*voice)) { PlaySound(*voice); }
    }
}

void unloadProjectileAudio(void)
{
    if (!neurosAudio.ready) { return; }
    for (int kind = 0; kind < 2; kind++) {
        for (int i = 0; i < SHOT_COUNT; i++) {
            if (projectileVoices[kind][i].stream.buffer != NULL) {
                StopSound(projectileVoices[kind][i]);
                UnloadSoundAlias(projectileVoices[kind][i]);
                projectileVoices[kind][i] = (Sound){0};
            }
        }
        if (projectileSources[kind].stream.buffer != NULL) {
            UnloadSound(projectileSources[kind]);
            projectileSources[kind] = (Sound){0};
        }
    }
}

void emitParticles(Game *game, Vector2 position, Color color, int count, float force)
{
    for (int n = 0; n < count; n++) {
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            Particle *particle = &game->particles[i];
            if (particle->life > 0.0f) { continue; }
            float angle = (float)(rand() % 6283) / 1000.0f;
            float speed = force * (0.25f + (float)(rand() % 100) / 100.0f);
            float lifetime = 0.25f + (float)(rand() % 55) / 100.0f;
            *particle = (Particle){position, {cosf(angle) * speed, sinf(angle) * speed}, color, lifetime, lifetime, 2.0f + (float)(rand() % 4)};
            break;
        }
    }
}

void loadLight(void)
{
    const char *relative = "shaders/galaga_light.fs";
    char local[1024];
    char parent[1024];
    snprintf(local, sizeof(local), "%s%s", GetApplicationDirectory(), relative);
    snprintf(parent, sizeof(parent), "%s../%s", GetApplicationDirectory(), relative);
    const char *path = NULL;
    if (FileExists(relative)) { path = relative; }
    else if (FileExists(local)) { path = local; }
    else if (FileExists(parent)) { path = parent; }
    if (path == NULL) { return; }
    lightShader = LoadShader(NULL, path);
    int radius = GetShaderLocation(lightShader, "glowRadius");
    int strength = GetShaderLocation(lightShader, "glowStrength");
    if (radius < 0 || strength < 0) { UnloadShader(lightShader); lightShader = (Shader){0}; return; }
    Image image = GenImageColor(2, 2, WHITE);
    if (image.data == NULL) { UnloadShader(lightShader); lightShader = (Shader){0}; return; }
    lightTexture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (lightTexture.id == 0) { UnloadShader(lightShader); lightShader = (Shader){0}; return; }
    float radiusValue = 3.1f;
    float strengthValue = 0.55f;
    SetShaderValue(lightShader, radius, &radiusValue, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightShader, strength, &strengthValue, SHADER_UNIFORM_FLOAT);
    lightReady = true;
}

void drawLight(Vector2 position, float radius, Color color)
{
    DrawTexturePro(lightTexture, (Rectangle){0, 0, 2, 2},
                   (Rectangle){position.x - radius, position.y - radius, radius * 2, radius * 2},
                   (Vector2){0, 0}, 0, color);
}

void drawPixelSprite(const char *const *sprite, int rows, int columns, Vector2 position, int scale, Color color)
{
    int left = (int)position.x - columns * scale / 2;
    int top = (int)position.y - rows * scale / 2;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < columns; x++) {
            if (sprite[y][x] == ' ') { continue; }
            Color pixel = sprite[y][x] == '2' ? TEXT : color;
            DrawRectangle(left + x * scale, top + y * scale, scale, scale, pixel);
        }
    }
}

Color shiftColor(Vector2 position, Vector2 velocity, Color base)
{
    DopplerState emitter = {{position.x, position.y, 0}, {velocity.x * COLOR_SPEED_SCALE, velocity.y * COLOR_SPEED_SCALE, 0}};
    DopplerState observer = {{WIDTH / 2.0, HEIGHT + 450.0, 0}, {0, 0, 0}};
    DopplerVec3 direction = {observer.position.x - emitter.position.x, observer.position.y - emitter.position.y, 0};
    double frequency = 1.0;
    double wavelength = 1.0;
    if (dopplerShift(DOPPLER_LIGHT, DOPPLER_LIGHT_SPEED, emitter, observer, direction, &frequency, &wavelength) != DOPPLER_OK) { return(base); }
    Color target = wavelength > 1.0 ? REDSHIFT : NEON_BLUE;
    float amount = (float)fmin(0.95, fabs(wavelength - 1.0) * 48.0);
    return((Color){(unsigned char)(base.r + (target.r - base.r) * amount), (unsigned char)(base.g + (target.g - base.g) * amount), (unsigned char)(base.b + (target.b - base.b) * amount), 255});
}

void drawGame(const Game *game)
{
    static const char *const bee[] = {
        "  1     1  ", "   1   1   ", "  1111111  ",
        " 11 121 11 ", "11111111111", "1 1111111 1",
        "1  11111  1", "   11 11   ", "  11   11  "
    };
    static const char *const boss[] = {
        "1   111   1", " 1  111  1 ", " 111111111 ",
        " 111121111 ", "11111111111", "11 11111 11",
        "1  11 11  1", "   1   1   ", "  11   11  "
    };
    static const char *const player[] = {
        "     2     ", "     1     ", "    111    ",
        "    121    ", "1   111   1", "11 11111 11",
        "11111111111", "11111111111", "11 11 11 11"
    };
    ClearBackground((Color){2, 3, 12, 255});
    for (int i = 0; i < 90; i++) {
        int x = (i * 173 + 31) % WIDTH;
        int y = 80 + (int)fmodf((float)((i * 227 + 17) % 760) + game->elapsed * (i % 3 + 1) * 16.0f, 760.0f);
        Color color = i % 5 == 0 ? NEON_VIOLET : i % 3 == 0 ? NEON_BLUE : (Color){130, 145, 180, 255};
        DrawRectangle(x, y, i % 6 == 0 ? 3 : 2, i % 6 == 0 ? 3 : 2, color);
    }
    rlPushMatrix();
    rlTranslatef(sinf(game->elapsed * 87.0f) * game->shake * 2.0f, cosf(game->elapsed * 103.0f) * game->shake * 2.0f, 0.0f);
    if (lightReady && lightEnabled) {
        BeginBlendMode(BLEND_ADDITIVE);
        BeginShaderMode(lightShader);
        for (int i = 0; i < ALIEN_COUNT; i++) {
            const Alien *alien = &game->aliens[i];
            if (!alien->alive) { continue; }
            Color color = shiftColor(alien->position, alien->velocity, alien->row == 0 ? REDSHIFT : NEON_VIOLET);
            float pulse = 1.0f + 0.12f * sinf(game->elapsed * 5.0f + i);
            drawLight(alien->position, (alien->diving ? 100.0f : 76.0f) * pulse, Fade(color, 0.42f));
        }
        for (int i = 0; i < SHOT_COUNT; i++) {
            const Shot *shot = &game->shots[i];
            if (!shot->active) { continue; }
            Color color = shiftColor(shot->position, shot->velocity, shot->friendly ? MINT : REDSHIFT);
            drawLight(shot->position, shot->friendly ? 35.0f : 43.0f, Fade(color, 0.45f));
        }
        if (!game->gameOver) {
            Color color = shiftColor(game->player, game->playerVelocity, MINT);
            drawLight(game->player, 75.0f, Fade(color, 0.38f));
        }
        EndShaderMode();
        EndBlendMode();
    }
    for (int i = 0; i < ALIEN_COUNT; i++) {
        const Alien *alien = &game->aliens[i];
        if (!alien->alive) { continue; }
        Color color = shiftColor(alien->position, alien->velocity, alien->row == 0 ? REDSHIFT : NEON_VIOLET);
        if (alien->row == 0 && alien->health == 1) { color = ColorAlpha(color, 0.55f); }
        drawPixelSprite(alien->row == 0 ? boss : bee, 9, 11, alien->position, 5, color);
    }
    for (int i = 0; i < SHOT_COUNT; i++) {
        const Shot *shot = &game->shots[i];
        if (!shot->active) { continue; }
        Color color = shiftColor(shot->position, shot->velocity, shot->friendly ? MINT : REDSHIFT);
        int x = (int)shot->position.x;
        int y = (int)shot->position.y;
        if (shot->friendly) {
            DrawRectangle(x - 3, y - 12, 6, 24, color);
            DrawRectangle(x - 1, y - 8, 2, 15, TEXT);
        }
        else {
            DrawRectangle(x - 6, y - 6, 12, 12, color);
            DrawRectangle(x - 2, y - 2, 4, 4, TEXT);
        }
    }
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        const Particle *particle = &game->particles[i];
        if (particle->life <= 0.0f) { continue; }
        int size = (int)fmaxf(2.0f, particle->size);
        DrawRectangle((int)particle->position.x, (int)particle->position.y, size, size,
                      Fade(particle->color, particle->life / particle->lifetime));
    }
    if (!game->gameOver && (game->invulnerability <= 0.0f || ((int)(game->invulnerability * 8) % 2 == 0))) {
        Color color = shiftColor(game->player, game->playerVelocity, MINT);
        drawPixelSprite(player, 9, 11, game->player, 5, color);
    }
    rlPopMatrix();
    DrawText("1UP", 100, 20, 25, MINT);
    DrawText(TextFormat("%06d", game->score), 100, 48, 28, TEXT);
    DrawText("NEUROS GALAGA", 475, 20, 26, NEON_VIOLET);
    DrawText(TextFormat("STAGE %02d", game->wave), 930, 20, 23, MINT);
    DrawText(TextFormat("LIVES %d", game->lives), 930, 49, 22, TEXT);
    if (game->waveNotice > 0.0f && !game->gameOver) {
        const char *message = TextFormat("STAGE %02d", game->wave);
        DrawText(message, (WIDTH - MeasureText(message, 26)) / 2, 110, 26, Fade(TEXT, fminf(1.0f, game->waveNotice)));
    }
    DrawText("MOVE: WASD / ARROWS   FIRE: SPACE   PAUSE: P   FX: F   SOUND: M   FULLSCREEN: F11", 45, 860, 14, MUTED);
    if (neurosAudio.muted || !neurosAudio.ready) { DrawText("SOUND OFF", 28, 92, 14, MUTED); }
    if (game->paused || game->gameOver) {
        DrawRectangle(0, 75, WIDTH, HEIGHT - 75, Fade(BACKGROUND, 0.88f));
        const char *title = game->gameOver ? "GAME OVER" : "PAUSE";
        DrawText(title, (WIDTH - MeasureText(title, 48)) / 2, 355, 48, game->gameOver ? REDSHIFT : MINT);
        if (game->gameOver) {
            const char *score = TextFormat("SCORE %06d", game->score);
            DrawText(score, (WIDTH - MeasureText(score, 23)) / 2, 420, 23, TEXT);
        }
        const char *instruction = game->gameOver ? "PUSH R TO PLAY" : "PUSH P TO CONTINUE";
        DrawText(instruction, (WIDTH - MeasureText(instruction, 20)) / 2, 475, 20, TEXT);
    }
}
