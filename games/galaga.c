#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <raylib.h>
#include "../shared/neuros_effect.c"
#include "../shared/neuros_audio.c"
#include "../physics/doppler.h"

#define WIDTH 1200
#define HEIGHT 900
#define ROWS 4
#define COLUMNS 9
#define ALIEN_COUNT (ROWS * COLUMNS)
#define SHOT_COUNT 96
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
    float diveTime;
    int row;
    int column;
} Alien;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    bool active;
    bool friendly;
} Shot;

typedef struct {
    Alien aliens[ALIEN_COUNT];
    Shot shots[SHOT_COUNT];
    Vector2 player;
    Vector2 playerVelocity;
    float formationTime;
    float diveTimer;
    float enemyShotTimer;
    float fireTimer;
    float invulnerability;
    int lives;
    int score;
    int wave;
    int remaining;
    bool paused;
    bool gameOver;
} Game;

static const Color BACKGROUND = {8, 11, 22, 255};
static const Color PANEL = {20, 28, 44, 255};
static const Color MINT = {52, 236, 168, 255};
static const Color NEON_VIOLET = {169, 92, 252, 255};
static const Color NEON_BLUE = {53, 191, 255, 255};
static const Color REDSHIFT = {255, 93, 114, 255};
static const Color TEXT = {229, 241, 245, 255};
static const Color MUTED = {154, 175, 192, 255};

void resetGame(Game *game);
void startWave(Game *game);
void updateGame(Game *game, float dt);
void updateAliens(Game *game, float dt);
void updateShots(Game *game, float dt);
void launchShot(Game *game, Vector2 position, Vector2 velocity, bool friendly, Vector2 sourceVelocity);
void loseLife(Game *game);
void drawGame(const Game *game);
Color shiftColor(Vector2 position, Vector2 velocity, Color base);
float soundPitch(Vector2 source, Vector2 velocity, Vector2 listener, Vector2 listenerVelocity);
void playShotSound(NeurosSound sound, float pitch);

int main(void)
{
    srand((unsigned int)time(NULL));
    if (VSYNC) { SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT); }
    else { SetConfigFlags(FLAG_MSAA_4X_HINT); }
    InitWindow(WIDTH, HEIGHT, "NeurOS | Galaga");
    if (!IsWindowReady()) { fprintf(stderr, "Could not open the game window.\n"); return(-1); }
    if (!VSYNC) { SetTargetFPS((FPS > 0 && FPS <= 300) ? FPS : 120); }
    initNeurosAudio();
    NeurosEffect effect = loadNeurosEffect(WIDTH, HEIGHT, 0.55f, (Vector3){0.66f, 0.36f, 0.99f});
    Game game = {0};
    resetGame(&game);
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_M)) { toggleNeurosAudio(); }
        if (IsKeyPressed(KEY_F) && effect.ready) { effect.enabled = !effect.enabled; }
        if (game.gameOver && IsKeyPressed(KEY_R)) { resetGame(&game); }
        else if (!game.gameOver && IsKeyPressed(KEY_P)) { game.paused = !game.paused; }
        if (!game.paused && !game.gameOver) {
            float dt = GetFrameTime();
            if (dt > 0.033f) { dt = 0.033f; }
            updateGame(&game, dt);
        }
        beginNeurosEffect(&effect);
        drawGame(&game);
        endNeurosEffect(&effect);
    }
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
    for (int i = 0; i < SHOT_COUNT; i++) { game->shots[i].active = false; }
    for (int i = 0; i < ALIEN_COUNT; i++) {
        int row = i / COLUMNS;
        int column = i % COLUMNS;
        game->aliens[i] = (Alien){(Vector2){280.0f + 80.0f * column, 155.0f + 55.0f * row}, {0.0f, 0.0f}, true, false, 0.0f, row, column};
    }
}

void updateGame(Game *game, float dt)
{
    int direction = (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
    float oldX = game->player.x;
    game->player.x = fmaxf(35.0f, fminf(WIDTH - 35.0f, game->player.x + direction * PLAYER_SPEED * dt));
    game->playerVelocity = (Vector2){dt > 0.0f ? (game->player.x - oldX) / dt : 0.0f, 0.0f};
    game->fireTimer -= dt;
    if (game->invulnerability > 0.0f) { game->invulnerability -= dt; }
    if (IsKeyDown(KEY_SPACE) && game->fireTimer <= 0.0f) {
        launchShot(game, (Vector2){game->player.x, game->player.y - 24.0f}, (Vector2){0, -700.0f}, true, game->playerVelocity);
        game->fireTimer = 0.18f;
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
            alien->diveTime += dt;
            alien->position.x += sinf(alien->diveTime * 2.5f + alien->column) * 160.0f * dt;
            alien->position.y += fminf(390.0f, 210.0f + 10.0f * game->wave) * dt;
            if (alien->position.y > HEIGHT + 40.0f) {
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
        shot->position.x += shot->velocity.x * dt;
        shot->position.y += shot->velocity.y * dt;
        if (shot->position.y < 80.0f || shot->position.y > HEIGHT || shot->position.x < 0 || shot->position.x > WIDTH) { shot->active = false; continue; }
        if (shot->friendly) {
            for (int j = 0; j < ALIEN_COUNT; j++) {
                Alien *alien = &game->aliens[j];
                if (alien->alive && CheckCollisionCircles(shot->position, 6.0f, alien->position, 21.0f)) {
                    alien->alive = false;
                    shot->active = false;
                    game->remaining--;
                    game->score += alien->diving ? 200 : 100;
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
        game->shots[i] = (Shot){position, velocity, true, friendly};
        Vector2 listener = friendly ? (Vector2){WIDTH / 2.0f, HEIGHT + 100.0f} : game->player;
        Vector2 listenerVelocity = friendly ? (Vector2){0} : game->playerVelocity;
        playShotSound(friendly ? SOUND_ROTATE : SOUND_SCORE, soundPitch(position, sourceVelocity, listener, listenerVelocity));
        return;
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
    float amount = (float)fmin(0.85, fabs(wavelength - 1.0) * 14.0);
    return((Color){(unsigned char)(base.r + (target.r - base.r) * amount), (unsigned char)(base.g + (target.g - base.g) * amount), (unsigned char)(base.b + (target.b - base.b) * amount), 255});
}

void drawGame(const Game *game)
{
    ClearBackground(BACKGROUND);
    for (int i = 0; i < 80; i++) {
        int x = (i * 173 + 31) % WIDTH;
        int y = 85 + (i * 227 + 17) % (HEIGHT - 120);
        DrawCircle(x, y, i % 5 == 0 ? 1.5f : 1.0f, (Color){59, 76, 111, 255});
    }
    DrawRectangle(20, 18, WIDTH - 40, 52, PANEL);
    DrawText("NEUROS // GALAGA", 38, 34, 20, MINT);
    DrawText(TextFormat("SCORE %06d", game->score), 410, 34, 20, TEXT);
    DrawText(TextFormat("WAVE %02d", game->wave), 720, 34, 20, TEXT);
    DrawText(TextFormat("LIVES %d", game->lives), 990, 34, 20, TEXT);
    for (int i = 0; i < ALIEN_COUNT; i++) {
        const Alien *alien = &game->aliens[i];
        if (!alien->alive) { continue; }
        Color color = shiftColor(alien->position, alien->velocity, NEON_VIOLET);
        float x = alien->position.x;
        float y = alien->position.y;
        DrawTriangle((Vector2){x - 22, y - 12}, (Vector2){x, y + 19}, (Vector2){x + 22, y - 12}, color);
        DrawRectangle((int)x - 12, (int)y - 12, 24, 11, color);
        DrawRectangle((int)x - 8, (int)y - 5, 5, 4, BACKGROUND);
        DrawRectangle((int)x + 3, (int)y - 5, 5, 4, BACKGROUND);
    }
    for (int i = 0; i < SHOT_COUNT; i++) {
        const Shot *shot = &game->shots[i];
        if (!shot->active) { continue; }
        Color color = shot->friendly ? MINT : REDSHIFT;
        DrawCircleV(shot->position, 7.0f, Fade(color, 0.24f));
        DrawRectangle((int)shot->position.x - 2, (int)shot->position.y - 8, 4, 16, color);
    }
    if (!game->gameOver && (game->invulnerability <= 0.0f || ((int)(game->invulnerability * 8) % 2 == 0))) {
        Color color = shiftColor(game->player, game->playerVelocity, MINT);
        float x = game->player.x;
        float y = game->player.y;
        DrawTriangle((Vector2){x, y - 24}, (Vector2){x - 24, y + 18}, (Vector2){x + 24, y + 18}, color);
        DrawRectangle((int)x - 4, (int)y - 5, 8, 12, TEXT);
        DrawRectangle((int)x - 17, (int)y + 17, 9, 5, NEON_VIOLET);
        DrawRectangle((int)x + 8, (int)y + 17, 9, 5, NEON_VIOLET);
    }
    DrawText("A / D OR ARROWS: MOVE   SPACE: FIRE   P: PAUSE   F: FX   M: SOUND   ESC: QUIT", 50, 855, 16, MUTED);
    DrawText("DOPPLER COLOR MAP: BLUE APPROACH / RED RECEDE  //  VELOCITY SCALED", 270, 825, 14, MUTED);
    if (neurosAudio.muted || !neurosAudio.ready) { DrawText("SOUND OFF", 38, 90, 14, MUTED); }
    if (game->paused || game->gameOver) {
        DrawRectangle(0, 70, WIDTH, HEIGHT - 70, Fade(BACKGROUND, 0.90f));
        const char *title = game->gameOver ? "GAME OVER" : "PAUSED";
        DrawText(title, (WIDTH - MeasureText(title, 54)) / 2, 355, 54, game->gameOver ? REDSHIFT : MINT);
        const char *instruction = game->gameOver ? "PRESS R TO RETRY" : "PRESS P TO RESUME";
        DrawText(instruction, (WIDTH - MeasureText(instruction, 20)) / 2, 435, 20, TEXT);
    }
}
