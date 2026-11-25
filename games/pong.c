#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <raylib.h>
#include "../shared/neuros_effect.c"
#include "../shared/neuros_start.c"
#include "../shared/neuros_audio.c"

#define WIDTH 1200
#define HEIGHT 900
#define FIELD_TOP 100.0f
#define FIELD_BOTTOM 830.0f
#define PADDLE_WIDTH 16.0f
#define PADDLE_HEIGHT 120.0f
#define PADDLE_SPEED 540.0f
#define BALL_RADIUS 9.0f
#define INITIAL_SPEED 440.0f
#define MAX_SPEED 850.0f
#define MAX_SCORE 7
#define STEP (1.0f / 240.0f)

#ifndef FPS
#define FPS 120
#endif

#ifndef VSYNC
#define VSYNC 0
#endif

typedef struct {
    Color background;
    Color panel;
    Color left;
    Color right;
    Color text;
    Color muted;
    Color grid;
} Theme;

typedef struct {
    Rectangle left;
    Rectangle right;
    Vector2 ball;
    Vector2 velocity;
    int leftScore;
    int rightScore;
    int rally;
    int winner;
    int serveDirection;
    float serveTimer;
    bool paused;
    bool dashed;
    int theme;
} Game;

static const Theme THEMES[] = {
    {{8, 9, 26, 255}, {18, 22, 48, 255}, {30, 235, 242, 255}, {255, 57, 166, 255}, {224, 240, 255, 255}, {126, 145, 177, 255}, {18, 23, 48, 255}},
    {{12, 10, 35, 255}, {30, 20, 55, 255}, {159, 255, 60, 255}, {255, 111, 91, 255}, {242, 235, 255, 255}, {165, 146, 190, 255}, {29, 22, 54, 255}},
    {{15, 18, 24, 255}, {34, 40, 50, 255}, {245, 245, 245, 255}, {190, 205, 220, 255}, {245, 245, 245, 255}, {150, 165, 180, 255}, {35, 41, 50, 255}}
};

void resetMatch(Game *game);
void prepareServe(Game *game, int direction);
void movePaddles(Game *game, float dt);
void updateBall(Game *game, float dt);
void bouncePaddle(Game *game, Rectangle paddle, int direction);
void drawGame(const Game *game);
void drawCentered(const char *text, int y, int size, Color color);

int main(void)
{
    srand((unsigned int)time(NULL));
    if (VSYNC) { SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT); }
    else { SetConfigFlags(FLAG_MSAA_4X_HINT); }
    InitWindow(WIDTH, HEIGHT, "NeurOS | Pong");
    if (!IsWindowReady()) { fprintf(stderr, "Could not open the game window.\n"); return(-1); }
    if (!VSYNC) { SetTargetFPS((FPS > 0 && FPS <= 300) ? FPS : 120); }

    initNeurosAudio();
    NeurosEffect effect = loadNeurosEffect(WIDTH, HEIGHT, 0.55f, (Vector3){0.20f, 0.93f, 0.66f});
    Game game = {0};
    game.dashed = true;
    resetMatch(&game);
    float accumulator = 0.0f;

    bool showTitle = true;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) { toggleNeurosFullscreen(&effect); }
        if (IsKeyPressed(KEY_M)) { toggleNeurosAudio(); }
        if (IsKeyPressed(KEY_F) && effect.ready) { effect.enabled = !effect.enabled; }
        if (showTitle) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { showTitle = false; }
            beginNeurosEffect(&effect);
            drawArcadeStart(ARCADE_PONG);
            endNeurosEffect(&effect);
            continue;
        }
        float dt = GetFrameTime();
        if (dt > 0.1f) { dt = 0.1f; }
        if (IsKeyPressed(KEY_TAB)) { game.theme = (game.theme + 1) % (int)(sizeof(THEMES) / sizeof(THEMES[0])); }
        if (IsKeyPressed(KEY_E)) { game.dashed = !game.dashed; }
        if (game.winner && IsKeyPressed(KEY_R)) { resetMatch(&game); accumulator = 0.0f; }
        if (!game.winner && IsKeyPressed(KEY_P)) { game.paused = !game.paused; accumulator = 0.0f; }

        int previousScore = game.leftScore + game.rightScore;
        int previousWinner = game.winner;
        int previousRally = game.rally;
        Vector2 previousVelocity = game.velocity;
        if (!game.winner && !game.paused) {
            movePaddles(&game, dt);
            if (game.serveTimer > 0.0f) {
                game.serveTimer -= dt;
                if (game.serveTimer < 0.0f) { game.serveTimer = 0.0f; }
                accumulator = 0.0f;
            }
            else {
                accumulator += dt;
                while (accumulator >= STEP && !game.winner && game.serveTimer <= 0.0f) {
                    updateBall(&game, STEP);
                    accumulator -= STEP;
                }
            }
        }
        else { accumulator = 0.0f; }

        if (game.winner && !previousWinner) { playNeurosSound(SOUND_WIN); }
        else if (game.leftScore + game.rightScore > previousScore) { playNeurosSound(SOUND_SCORE); }
        else if (game.rally > previousRally || game.velocity.y * previousVelocity.y < 0.0f) { playNeurosSound(SOUND_HIT); }

        beginNeurosEffect(&effect);
        drawGame(&game);
        endNeurosEffect(&effect);
    }
    unloadNeurosAudio();
    unloadNeurosEffect(&effect);
    CloseWindow();
    return(0);
}

void resetMatch(Game *game)
{
    int theme = game->theme;
    bool dashed = game->dashed;
    *game = (Game){0};
    game->theme = theme;
    game->dashed = dashed;
    game->left = (Rectangle){50.0f, (FIELD_TOP + FIELD_BOTTOM - PADDLE_HEIGHT) / 2.0f, PADDLE_WIDTH, PADDLE_HEIGHT};
    game->right = (Rectangle){WIDTH - 50.0f - PADDLE_WIDTH, game->left.y, PADDLE_WIDTH, PADDLE_HEIGHT};
    prepareServe(game, rand() % 2 ? 1 : -1);
}

void prepareServe(Game *game, int direction)
{
    float vertical = ((rand() % 81) - 40) / 100.0f;
    game->ball = (Vector2){WIDTH / 2.0f, (FIELD_TOP + FIELD_BOTTOM) / 2.0f};
    game->velocity = (Vector2){direction * INITIAL_SPEED * sqrtf(1.0f - vertical * vertical), INITIAL_SPEED * vertical};
    game->serveDirection = direction;
    game->serveTimer = 1.5f;
    game->rally = 0;
}

void movePaddles(Game *game, float dt)
{
    int leftDirection = IsKeyDown(KEY_S) - IsKeyDown(KEY_W);
    int rightDirection = IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP);
    game->left.y = fmaxf(FIELD_TOP, fminf(FIELD_BOTTOM - game->left.height, game->left.y + leftDirection * PADDLE_SPEED * dt));
    game->right.y = fmaxf(FIELD_TOP, fminf(FIELD_BOTTOM - game->right.height, game->right.y + rightDirection * PADDLE_SPEED * dt));
}

void bouncePaddle(Game *game, Rectangle paddle, int direction)
{
    float hit = (game->ball.y - (paddle.y + paddle.height / 2.0f)) / (paddle.height / 2.0f);
    hit = fmaxf(-1.0f, fminf(1.0f, hit));
    float angle = hit * 60.0f * DEG2RAD;
    float speed = fminf(hypotf(game->velocity.x, game->velocity.y) * 1.06f, MAX_SPEED);
    game->velocity = (Vector2){direction * cosf(angle) * speed, sinf(angle) * speed};
    if (direction > 0) { game->ball.x = paddle.x + paddle.width + BALL_RADIUS + 0.01f; }
    else { game->ball.x = paddle.x - BALL_RADIUS - 0.01f; }
    game->rally++;
}

void updateBall(Game *game, float dt)
{
    game->ball.x += game->velocity.x * dt;
    game->ball.y += game->velocity.y * dt;

    if (game->ball.y - BALL_RADIUS < FIELD_TOP) {
        game->ball.y = FIELD_TOP + BALL_RADIUS;
        game->velocity.y = fabsf(game->velocity.y);
    }
    else if (game->ball.y + BALL_RADIUS > FIELD_BOTTOM) {
        game->ball.y = FIELD_BOTTOM - BALL_RADIUS;
        game->velocity.y = -fabsf(game->velocity.y);
    }
    if (game->velocity.x < 0.0f && CheckCollisionCircleRec(game->ball, BALL_RADIUS, game->left)) { bouncePaddle(game, game->left, 1); }
    else if (game->velocity.x > 0.0f && CheckCollisionCircleRec(game->ball, BALL_RADIUS, game->right)) { bouncePaddle(game, game->right, -1); }

    if (game->ball.x + BALL_RADIUS < 0.0f) {
        game->rightScore++;
        if (game->rightScore >= MAX_SCORE) { game->winner = 2; }
        else { prepareServe(game, -1); }
    }
    else if (game->ball.x - BALL_RADIUS > WIDTH) {
        game->leftScore++;
        if (game->leftScore >= MAX_SCORE) { game->winner = 1; }
        else { prepareServe(game, 1); }
    }
}

void drawCentered(const char *text, int y, int size, Color color)
{
    DrawText(text, (WIDTH - MeasureText(text, size)) / 2, y, size, color);
}

void drawGame(const Game *game)
{
    Theme theme = THEMES[game->theme];
    ClearBackground(theme.background);
    for (int x = 0; x < WIDTH; x += 80) { DrawLine(x, (int)FIELD_TOP, x, (int)FIELD_BOTTOM, theme.grid); }
    for (int y = (int)FIELD_TOP; y < FIELD_BOTTOM; y += 80) { DrawLine(0, y, WIDTH, y, theme.grid); }
    DrawRectangle(20, 20, WIDTH - 40, 55, theme.panel);
    DrawText("NEUROS // PONG", 38, 37, 20, theme.left);
    DrawText("FIRST TO 7", WIDTH / 2 - MeasureText("FIRST TO 7", 20) / 2, 37, 20, theme.text);
    if (neurosAudio.muted || !neurosAudio.ready) { DrawText("SOUND OFF", 820, 37, 16, theme.muted); }
    DrawText(TextFormat("RALLY %02d", game->rally), 1030, 37, 20, theme.muted);
    DrawLine(0, (int)FIELD_TOP, WIDTH, (int)FIELD_TOP, theme.muted);
    DrawLine(0, (int)FIELD_BOTTOM, WIDTH, (int)FIELD_BOTTOM, theme.muted);

    if (game->dashed) {
        for (int y = (int)FIELD_TOP + 15; y < FIELD_BOTTOM - 15; y += 38) { DrawRectangle(WIDTH / 2 - 2, y, 4, 20, theme.muted); }
    }
    const char *leftScore = TextFormat("%d", game->leftScore);
    DrawText(leftScore, WIDTH / 4 - MeasureText(leftScore, 64) / 2, 125, 64, Fade(theme.left, 0.7f));
    const char *rightScore = TextFormat("%d", game->rightScore);
    DrawText(rightScore, 3 * WIDTH / 4 - MeasureText(rightScore, 64) / 2, 125, 64, Fade(theme.right, 0.7f));
    DrawRectangleRec(game->left, theme.left);
    DrawRectangleRec(game->right, theme.right);
    DrawCircleV(game->ball, BALL_RADIUS + 6.0f, Fade(theme.text, 0.2f));
    DrawCircleV(game->ball, BALL_RADIUS, theme.text);

    if (!game->winner && !game->paused && game->serveTimer > 0.0f) {
        drawCentered(game->serveDirection < 0 ? "SERVING TO PLAYER 1" : "SERVING TO PLAYER 2", 535, 20, theme.muted);
    }
    if (game->paused || game->winner) {
        DrawRectangle(0, (int)FIELD_TOP, WIDTH, (int)(FIELD_BOTTOM - FIELD_TOP), Fade(theme.background, 0.88f));
        if (game->paused) {
            drawCentered("PAUSED", 390, 54, theme.left);
            drawCentered("PRESS P TO RESUME", 460, 20, theme.text);
        }
        else {
            drawCentered(game->winner == 1 ? "PLAYER 1 WINS" : "PLAYER 2 WINS", 375, 54, game->winner == 1 ? theme.left : theme.right);
            drawCentered(TextFormat("%d  -  %d", game->leftScore, game->rightScore), 450, 32, theme.text);
            drawCentered("PRESS R TO REMATCH", 500, 20, theme.text);
        }
    }
    DrawText("P1: W / S    P2: UP / DOWN    P: PAUSE    TAB: THEME    E: LINE    F: FX    M: SOUND    F11: FULLSCREEN    ESC: QUIT", 38, 860, 14, theme.muted);
}
