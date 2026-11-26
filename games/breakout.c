#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <raylib.h>
#include "../shared/neuros_effect.c"
#include "../shared/neuros_start.c"
#include "../shared/neuros_audio.c"
#include "../shared/neuros_visual.c"

#define WIDTH 1200
#define HEIGHT 900
#define BRICK_ROWS 6
#define BRICK_COLUMNS 12
#define BRICK_COUNT (BRICK_ROWS * BRICK_COLUMNS)
#define PADDLE_WIDTH 180.0f
#define PADDLE_HEIGHT 16.0f
#define PADDLE_SPEED 640.0f
#define BALL_RADIUS 9.0f
#define BALL_SPEED 440.0f
#define STEP (1.0f / 240.0f)

#ifndef FPS
#define FPS 120
#endif

#ifndef VSYNC
#define VSYNC 0
#endif

typedef enum {
    READY,
    PLAYING,
    PAUSED,
    WON,
    LOST
} Phase;

typedef struct {
    Rectangle rect;
    Color color;
    bool active;
} Brick;

typedef struct {
    Rectangle paddle;
    Vector2 ball;
    Vector2 velocity;
    Brick bricks[BRICK_COUNT];
    Phase phase;
    int lives;
    int score;
    int combo;
    int remaining;
} Game;

static const Color BACKGROUND = {8, 9, 26, 255};
static const Color PANEL = {18, 22, 48, 255};
static const Color CYAN = {30, 235, 242, 255};
static const Color NEON_PINK = {255, 57, 166, 255};
static const Color TEXT = {224, 240, 255, 255};
static const Color MUTED = {126, 145, 177, 255};

void resetGame(Game *game);
void attachBall(Game *game);
void updateGame(Game *game, float dt);
void updateBall(Game *game, float dt);
void collideBricks(Game *game);
void drawGame(const Game *game);
void drawCentered(const char *text, int y, int size, Color color);

int main(void)
{
    if (VSYNC) { SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT); }
    else { SetConfigFlags(FLAG_MSAA_4X_HINT); }
    InitWindow(WIDTH, HEIGHT, "NeurOS | Breakout");
    if (!IsWindowReady()) { fprintf(stderr, "Could not open the game window.\n"); return(-1); }
    if (!VSYNC) { SetTargetFPS((FPS > 0 && FPS <= 300) ? FPS : 120); }

    initNeurosAudio();
    NeurosEffect effect = loadNeurosEffect(WIDTH, HEIGHT, 0.65f, (Vector3){0.66f, 0.36f, 0.99f});
    Game game = {0};
    resetGame(&game);
    float accumulator = 0.0f;

    bool showTitle = true;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) { toggleNeurosFullscreen(&effect); }
        if (IsKeyPressed(KEY_M)) { toggleNeurosAudio(); }
        if (IsKeyPressed(KEY_F) && effect.ready) { effect.enabled = !effect.enabled; }
        if (showTitle) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { showTitle = false; }
            beginNeurosEffect(&effect);
            drawRetroStart(ARCADE_BREAKOUT);
            endNeurosEffect(&effect);
            continue;
        }
        float dt = GetFrameTime();
        if (dt > 0.1f) { dt = 0.1f; }

        if ((game.phase == WON || game.phase == LOST) && IsKeyPressed(KEY_R)) {
            resetGame(&game);
            accumulator = 0.0f;
        }
        else if (IsKeyPressed(KEY_P)) {
            if (game.phase == PLAYING) { game.phase = PAUSED; }
            else if (game.phase == PAUSED) { game.phase = PLAYING; }
            accumulator = 0.0f;
        }

        int previousBricks = game.remaining;
        int previousLives = game.lives;
        Phase previousPhase = game.phase;
        Vector2 previousVelocity = game.velocity;
        if (game.phase == READY || game.phase == PLAYING) { updateGame(&game, dt); }
        if (game.phase == PLAYING) {
            accumulator += dt;
            while (accumulator >= STEP && game.phase == PLAYING) {
                updateBall(&game, STEP);
                accumulator -= STEP;
            }
        }
        else { accumulator = 0.0f; }

        if (game.phase == WON && previousPhase != WON) { playNeurosSound(SOUND_WIN); }
        else if (game.phase == LOST && previousPhase != LOST) { playNeurosSound(SOUND_LOSE); }
        else if (game.lives < previousLives) { playNeurosSound(SOUND_LOSE); }
        else if (game.remaining < previousBricks) { playNeurosSound(SOUND_SCORE); }
        else if (previousPhase == PLAYING && game.phase == PLAYING &&
                 (game.velocity.x * previousVelocity.x < 0.0f || game.velocity.y * previousVelocity.y < 0.0f)) { playNeurosSound(SOUND_HIT); }

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
    static const Color colors[BRICK_ROWS] = {
        {255, 57, 166, 255}, {236, 81, 203, 255}, {178, 104, 255, 255},
        {117, 130, 255, 255}, {45, 181, 255, 255}, {30, 235, 242, 255}
    };
    const float brickWidth = 80.0f;
    const float brickHeight = 24.0f;
    const float gap = 6.0f;
    const float left = (WIDTH - BRICK_COLUMNS * brickWidth - (BRICK_COLUMNS - 1) * gap) / 2.0f;

    *game = (Game){0};
    game->paddle = (Rectangle){(WIDTH - PADDLE_WIDTH) / 2.0f, 796.0f, PADDLE_WIDTH, PADDLE_HEIGHT};
    game->lives = 3;
    game->remaining = BRICK_COUNT;
    game->phase = READY;
    attachBall(game);

    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int col = 0; col < BRICK_COLUMNS; col++) {
            Brick *brick = &game->bricks[row * BRICK_COLUMNS + col];
            brick->rect = (Rectangle){left + col * (brickWidth + gap), 150.0f + row * (brickHeight + gap), brickWidth, brickHeight};
            brick->color = colors[row];
            brick->active = true;
        }
    }
}

void attachBall(Game *game)
{
    game->ball = (Vector2){game->paddle.x + game->paddle.width / 2.0f, game->paddle.y - BALL_RADIUS - 1.0f};
    game->velocity = (Vector2){BALL_SPEED * 0.35f, -BALL_SPEED * sqrtf(1.0f - 0.35f * 0.35f)};
}

void updateGame(Game *game, float dt)
{
    int direction = (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    game->paddle.x += direction * PADDLE_SPEED * dt;
    if (game->paddle.x < 20.0f) { game->paddle.x = 20.0f; }
    if (game->paddle.x + game->paddle.width > WIDTH - 20.0f) { game->paddle.x = WIDTH - 20.0f - game->paddle.width; }

    if (game->phase == READY) {
        attachBall(game);
        if (IsKeyPressed(KEY_SPACE)) { game->phase = PLAYING; }
    }
}

void updateBall(Game *game, float dt)
{
    game->ball.x += game->velocity.x * dt;
    game->ball.y += game->velocity.y * dt;

    if (game->ball.x - BALL_RADIUS < 20.0f) {
        game->ball.x = 20.0f + BALL_RADIUS;
        game->velocity.x = fabsf(game->velocity.x);
    }
    else if (game->ball.x + BALL_RADIUS > WIDTH - 20.0f) {
        game->ball.x = WIDTH - 20.0f - BALL_RADIUS;
        game->velocity.x = -fabsf(game->velocity.x);
    }
    if (game->ball.y - BALL_RADIUS < 85.0f) {
        game->ball.y = 85.0f + BALL_RADIUS;
        game->velocity.y = fabsf(game->velocity.y);
    }
    if (game->ball.y - BALL_RADIUS > HEIGHT) {
        game->lives--;
        game->combo = 0;
        if (game->lives == 0) { game->phase = LOST; }
        else { game->phase = READY; attachBall(game); }
        return;
    }

    if (game->velocity.y > 0.0f && CheckCollisionCircleRec(game->ball, BALL_RADIUS, game->paddle)) {
        float hit = (game->ball.x - (game->paddle.x + game->paddle.width / 2.0f)) / (game->paddle.width / 2.0f);
        hit = fmaxf(-1.0f, fminf(1.0f, hit));
        float angle = hit * 65.0f * DEG2RAD;
        float speed = fminf(BALL_SPEED + game->combo * 8.0f, 650.0f);
        game->ball.y = game->paddle.y - BALL_RADIUS - 0.01f;
        game->velocity = (Vector2){sinf(angle) * speed, -cosf(angle) * speed};
        game->combo = 0;
    }
    collideBricks(game);
}

void collideBricks(Game *game)
{
    for (int i = 0; i < BRICK_COUNT; i++) {
        Brick *brick = &game->bricks[i];
        if (!brick->active || !CheckCollisionCircleRec(game->ball, BALL_RADIUS, brick->rect)) { continue; }

        float closestX = fmaxf(brick->rect.x, fminf(game->ball.x, brick->rect.x + brick->rect.width));
        float closestY = fmaxf(brick->rect.y, fminf(game->ball.y, brick->rect.y + brick->rect.height));
        float dx = game->ball.x - closestX;
        float dy = game->ball.y - closestY;
        if (dx == 0.0f && dy == 0.0f) {
            float left = game->ball.x - brick->rect.x;
            float right = brick->rect.x + brick->rect.width - game->ball.x;
            float top = game->ball.y - brick->rect.y;
            float bottom = brick->rect.y + brick->rect.height - game->ball.y;
            float minimum = fminf(fminf(left, right), fminf(top, bottom));
            if (minimum == left) { dx = -1.0f; }
            else if (minimum == right) { dx = 1.0f; }
            else if (minimum == top) { dy = -1.0f; }
            else { dy = 1.0f; }
        }
        if (fabsf(dx) > fabsf(dy)) { game->velocity.x = copysignf(fabsf(game->velocity.x), dx); }
        else { game->velocity.y = copysignf(fabsf(game->velocity.y), dy); }

        brick->active = false;
        game->remaining--;
        game->combo++;
        game->score += 100 * (game->combo < 5 ? game->combo : 5);
        if (game->remaining == 0) { game->phase = WON; }
        return;
    }
}

void drawCentered(const char *text, int y, int size, Color color)
{
    DrawText(text, (WIDTH - MeasureText(text, size)) / 2, y, size, color);
}

void drawGame(const Game *game)
{
    drawNeurosBackdrop(WIDTH, HEIGHT, BACKGROUND, CYAN);
    drawNeurosPanel((Rectangle){20, 20, WIDTH - 40, 55}, PANEL, CYAN);
    DrawText("NEUROS // BREAKOUT", 38, 37, 20, CYAN);
    DrawText(TextFormat("SCORE %06d", game->score), 460, 37, 20, TEXT);
    DrawText(TextFormat("LIVES %d", game->lives), 780, 37, 20, TEXT);
    if (neurosAudio.muted || !neurosAudio.ready) { DrawText("SOUND OFF", 600, 63, 12, MUTED); }
    DrawText(TextFormat("BRICKS %02d", game->remaining), 985, 37, 20, TEXT);
    DrawLine(20, 85, WIDTH - 20, 85, CYAN);
    DrawLine(20, 85, 20, HEIGHT, CYAN);
    DrawLine(WIDTH - 20, 85, WIDTH - 20, HEIGHT, CYAN);

    for (int i = 0; i < BRICK_COUNT; i++) {
        const Brick *brick = &game->bricks[i];
        if (!brick->active) { continue; }
        DrawRectangleRec(brick->rect, brick->color);
        DrawRectangle((int)brick->rect.x + 4, (int)brick->rect.y + (int)brick->rect.height - 4, (int)brick->rect.width - 8, 2, Fade(BLACK, 0.3f));
    }
    DrawRectangleRec(game->paddle, CYAN);
    drawNeurosOrb(game->ball, BALL_RADIUS, NEON_PINK);

    if (game->combo > 1 && game->phase == PLAYING) { DrawText(TextFormat("COMBO x%d", game->combo < 5 ? game->combo : 5), 38, 100, 20, NEON_PINK); }
    if (game->phase == READY) {
        drawCentered("PRESS SPACE TO LAUNCH", 585, 26, TEXT);
        drawCentered("CHAIN BRICKS FOR UP TO 5X SCORE", 620, 16, MUTED);
    }
    else if (game->phase == PAUSED || game->phase == WON || game->phase == LOST) {
        DrawRectangle(0, 85, WIDTH, HEIGHT - 85, (Color){8, 9, 26, 210});
        if (game->phase == PAUSED) {
            drawCentered("PAUSED", 395, 54, CYAN);
            drawCentered("PRESS P TO RESUME", 465, 20, TEXT);
        }
        else {
            drawCentered(game->phase == WON ? "GRID CLEARED" : "GAME OVER", 375, 54, game->phase == WON ? CYAN : NEON_PINK);
            drawCentered(TextFormat("FINAL SCORE  %06d", game->score), 450, 24, TEXT);
            drawCentered("PRESS R TO PLAY AGAIN", 495, 20, TEXT);
        }
    }
    DrawText("A / D OR ARROWS: MOVE    SPACE: LAUNCH    P: PAUSE    F: FX    M: SOUND    F11: FULLSCREEN    ESC: QUIT", 38, 860, 14, MUTED);
}
