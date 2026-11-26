#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <raylib.h>
#include "../shared/neuros_effect.c"
#include "../shared/neuros_start.c"
#include "../shared/neuros_audio.c"
#include "../shared/neuros_visual.c"

#define WIDTH 1200
#define HEIGHT 900
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define CELL_SIZE 36
#define BOARD_X 420
#define BOARD_Y 85
#define PIECES 7

#ifndef FPS
#define FPS 120
#endif

#ifndef VSYNC
#define VSYNC 0
#endif

typedef struct {
    int type;
    int rotation;
    int x;
    int y;
} Piece;

typedef struct {
    int board[BOARD_HEIGHT][BOARD_WIDTH];
    int bag[PIECES];
    int bagIndex;
    int next;
    int held;
    Piece active;
    int score;
    int lines;
    int level;
    float gravityTimer;
    float moveTimer;
    int moveDirection;
    bool holdUsed;
    bool paused;
    bool gameOver;
} Game;

static const unsigned short SHAPES[PIECES] = {
    0x00F0, 0x0066, 0x0072, 0x0036, 0x0063, 0x0071, 0x0074
};

static const Color PIECE_COLORS[PIECES] = {
    {53, 191, 255, 255}, {255, 188, 85, 255}, {169, 92, 252, 255},
    {52, 236, 168, 255}, {255, 93, 114, 255}, {105, 129, 255, 255}, {215, 133, 255, 255}
};

static const Color BACKGROUND = {8, 11, 22, 255};
static const Color PANEL = {20, 28, 44, 255};
static const Color NEON_VIOLET = {169, 92, 252, 255};
static const Color MINT = {52, 236, 168, 255};
static const Color TEXT = {229, 241, 245, 255};
static const Color MUTED = {154, 175, 192, 255};

bool pieceCell(int type, int rotation, int x, int y);
bool fits(const Game *game, Piece piece);
int takePiece(Game *game);
void resetGame(Game *game);
void spawnPiece(Game *game, int type);
void holdPiece(Game *game);
bool movePiece(Game *game, int dx, int dy);
void rotatePiece(Game *game, int direction);
void lockPiece(Game *game);
void hardDrop(Game *game);
void updateGame(Game *game, float dt);
void drawGame(const Game *game);
void drawBlock(int x, int y, int size, Color color, bool ghost);
void drawPreview(int type, int x, int y);
void drawCentered(const char *text, int x, int y, int width, int size, Color color);

int main(void)
{
    srand((unsigned int)time(NULL));
    if (VSYNC) { SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT); }
    else { SetConfigFlags(FLAG_MSAA_4X_HINT); }
    InitWindow(WIDTH, HEIGHT, "NeurOS | Tetris");
    if (!IsWindowReady()) { fprintf(stderr, "Could not open the game window.\n"); return(-1); }
    if (!VSYNC) { SetTargetFPS((FPS > 0 && FPS <= 300) ? FPS : 120); }

    initNeurosAudio();
    NeurosEffect effect = loadNeurosEffect(WIDTH, HEIGHT, 0.50f, (Vector3){0.21f, 0.75f, 1.0f});
    Game game = {0};
    resetGame(&game);
    bool showTitle = true;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) { toggleNeurosFullscreen(&effect); }
        if (IsKeyPressed(KEY_M)) { toggleNeurosAudio(); }
        if (IsKeyPressed(KEY_F) && effect.ready) { effect.enabled = !effect.enabled; }
        if (showTitle) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { showTitle = false; }
            beginNeurosEffect(&effect);
            drawRetroStart(ARCADE_TETRIS);
            endNeurosEffect(&effect);
            continue;
        }
        float dt = GetFrameTime();
        if (dt > 0.1f) { dt = 0.1f; }
        if (game.gameOver && IsKeyPressed(KEY_R)) { resetGame(&game); }
        else if (!game.gameOver && IsKeyPressed(KEY_P)) {
            game.paused = !game.paused;
            game.moveDirection = 0;
            game.moveTimer = 0.0f;
        }
        if (!game.paused && !game.gameOver) { updateGame(&game, dt); }
        beginNeurosEffect(&effect);
        drawGame(&game);
        endNeurosEffect(&effect);
    }
    unloadNeurosAudio();
    unloadNeurosEffect(&effect);
    CloseWindow();
    return(0);
}

bool pieceCell(int type, int rotation, int x, int y)
{
    if (type == 1) { rotation = 0; }
    int size = type == 0 ? 4 : 3;
    for (int i = 0; i < rotation; i++) {
        int oldX = x;
        x = y;
        y = size - 1 - oldX;
    }
    if (x < 0 || y < 0 || x >= size || y >= size) { return(false); }
    return((SHAPES[type] & (1u << (y * 4 + x))) != 0);
}

bool fits(const Game *game, Piece piece)
{
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (!pieceCell(piece.type, piece.rotation, x, y)) { continue; }
            int bx = piece.x + x;
            int by = piece.y + y;
            if (bx < 0 || bx >= BOARD_WIDTH || by >= BOARD_HEIGHT) { return(false); }
            if (by >= 0 && game->board[by][bx] != 0) { return(false); }
        }
    }
    return(true);
}

int takePiece(Game *game)
{
    if (game->bagIndex >= PIECES) {
        for (int i = 0; i < PIECES; i++) { game->bag[i] = i; }
        for (int i = PIECES - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            int temp = game->bag[i];
            game->bag[i] = game->bag[j];
            game->bag[j] = temp;
        }
        game->bagIndex = 0;
    }
    return(game->bag[game->bagIndex++]);
}

void spawnPiece(Game *game, int type)
{
    game->active = (Piece){type, 0, 3, 0};
    game->gravityTimer = 0.0f;
    if (!fits(game, game->active)) { game->gameOver = true; playNeurosSound(SOUND_LOSE); }
}

void resetGame(Game *game)
{
    *game = (Game){0};
    game->bagIndex = PIECES;
    game->held = -1;
    game->level = 1;
    int first = takePiece(game);
    game->next = takePiece(game);
    spawnPiece(game, first);
}

void holdPiece(Game *game)
{
    if (game->holdUsed) { return; }
    int current = game->active.type;
    if (game->held < 0) {
        spawnPiece(game, game->next);
        game->next = takePiece(game);
    }
    else { spawnPiece(game, game->held); }
    game->held = current;
    game->holdUsed = true;
    if (!game->gameOver) { playNeurosSound(SOUND_ROTATE); }
}

bool movePiece(Game *game, int dx, int dy)
{
    Piece candidate = game->active;
    candidate.x += dx;
    candidate.y += dy;
    if (!fits(game, candidate)) { return(false); }
    game->active = candidate;
    if (dx != 0) { playNeurosSound(SOUND_MOVE); }
    return(true);
}

void rotatePiece(Game *game, int direction)
{
    static const int kicks[][2] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}, {-1, -1}, {1, -1}, {0, -2}};
    Piece candidate = game->active;
    candidate.rotation = (candidate.rotation + direction + 4) % 4;
    for (unsigned int i = 0; i < sizeof(kicks) / sizeof(kicks[0]); i++) {
        candidate.x = game->active.x + kicks[i][0];
        candidate.y = game->active.y + kicks[i][1];
        if (fits(game, candidate)) { game->active = candidate; playNeurosSound(SOUND_ROTATE); return; }
    }
}

void lockPiece(Game *game)
{
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (!pieceCell(game->active.type, game->active.rotation, x, y)) { continue; }
            int by = game->active.y + y;
            if (by < 0) { game->gameOver = true; playNeurosSound(SOUND_LOSE); return; }
        }
    }
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (pieceCell(game->active.type, game->active.rotation, x, y)) {
                game->board[game->active.y + y][game->active.x + x] = game->active.type + 1;
            }
        }
    }
    int cleared = 0;
    for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (game->board[y][x] == 0) { full = false; break; }
        }
        if (full) {
            for (int row = y; row > 0; row--) {
                for (int x = 0; x < BOARD_WIDTH; x++) { game->board[row][x] = game->board[row - 1][x]; }
            }
            for (int x = 0; x < BOARD_WIDTH; x++) { game->board[0][x] = 0; }
            cleared++;
            y++;
        }
    }
    static const int points[] = {0, 100, 300, 500, 800};
    if (cleared > 0) { playNeurosSound(SOUND_CLEAR); }
    else { playNeurosSound(SOUND_HIT); }
    game->score += points[cleared] * game->level;
    game->lines += cleared;
    game->level = game->lines / 10 + 1;
    game->holdUsed = false;
    spawnPiece(game, game->next);
    game->next = takePiece(game);
}

void hardDrop(Game *game)
{
    while (movePiece(game, 0, 1)) { game->score += 2; }
    lockPiece(game);
}

void updateGame(Game *game, float dt)
{
    if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) { holdPiece(game); }
    if (game->gameOver) { return; }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_X)) { rotatePiece(game, 1); }
    if (IsKeyPressed(KEY_Z)) { rotatePiece(game, -1); }

    int direction = IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT);
    if (direction != game->moveDirection) {
        game->moveDirection = direction;
        game->moveTimer = 0.0f;
        if (direction != 0) { movePiece(game, direction, 0); }
    }
    else if (direction != 0) {
        game->moveTimer += dt;
        if (game->moveTimer >= 0.17f) {
            movePiece(game, direction, 0);
            game->moveTimer -= 0.05f;
        }
    }
    if (IsKeyPressed(KEY_SPACE)) { hardDrop(game); return; }

    float interval = fmaxf(0.07f, 0.8f * powf(0.85f, (float)(game->level - 1)));
    bool softDrop = IsKeyDown(KEY_DOWN);
    if (softDrop) { interval = fminf(interval, 0.045f); }
    game->gravityTimer += dt;
    while (game->gravityTimer >= interval) {
        game->gravityTimer -= interval;
        if (!movePiece(game, 0, 1)) { lockPiece(game); return; }
        if (softDrop) { game->score++; }
    }
}

void drawBlock(int x, int y, int size, Color color, bool ghost)
{
    Rectangle rect = {(float)x + 1, (float)y + 1, (float)size - 2, (float)size - 2};
    if (ghost) { DrawRectangleLinesEx(rect, 2.0f, Fade(color, 0.55f)); return; }
    DrawRectangleRec(rect, color);
    DrawRectangle(x + 4, y + 4, size - 8, 3, Fade(WHITE, 0.5f));
    DrawRectangle(x + 4, y + 4, 3, size - 8, Fade(WHITE, 0.3f));
    DrawRectangle(x + 4, y + size - 7, size - 8, 3, Fade(BLACK, 0.45f));
    DrawRectangle(x + size - 7, y + 4, 3, size - 8, Fade(BLACK, 0.35f));
}

void drawPreview(int type, int x, int y)
{
    if (type < 0) { return; }
    int minX = 4;
    int maxX = 0;
    int minY = 4;
    int maxY = 0;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            if (!pieceCell(type, 0, col, row)) { continue; }
            if (col < minX) { minX = col; }
            if (col > maxX) { maxX = col; }
            if (row < minY) { minY = row; }
            if (row > maxY) { maxY = row; }
        }
    }
    int size = 24;
    int left = x + (180 - (maxX - minX + 1) * size) / 2;
    int top = y + (90 - (maxY - minY + 1) * size) / 2;
    for (int row = minY; row <= maxY; row++) {
        for (int col = minX; col <= maxX; col++) {
            if (pieceCell(type, 0, col, row)) { drawBlock(left + (col - minX) * size, top + (row - minY) * size, size, PIECE_COLORS[type], false); }
        }
    }
}

void drawCentered(const char *text, int x, int y, int width, int size, Color color)
{
    DrawText(text, x + (width - MeasureText(text, size)) / 2, y, size, color);
}

void drawGame(const Game *game)
{
    drawNeurosBackdrop(WIDTH, HEIGHT, BACKGROUND, NEON_VIOLET);
    drawNeurosPanel((Rectangle){20, 18, WIDTH - 40, 48}, PANEL, MINT);
    DrawText("NEUROS // TETRIS", 38, 32, 20, MINT);
    DrawText("BLOCK STACKING PROTOCOL", 880, 34, 14, MUTED);
    if (neurosAudio.muted || !neurosAudio.ready) { DrawText("SOUND OFF", 720, 34, 14, MUTED); }
    DrawRectangle(BOARD_X - 4, BOARD_Y - 4, BOARD_WIDTH * CELL_SIZE + 8, BOARD_HEIGHT * CELL_SIZE + 8, PANEL);
    DrawRectangle(BOARD_X, BOARD_Y, BOARD_WIDTH * CELL_SIZE, BOARD_HEIGHT * CELL_SIZE, BACKGROUND);
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            int cell = game->board[y][x];
            if (cell) { drawBlock(BOARD_X + x * CELL_SIZE, BOARD_Y + y * CELL_SIZE, CELL_SIZE, PIECE_COLORS[cell - 1], false); }
        }
    }
    if (!game->gameOver) {
        Piece ghost = game->active;
        Piece next = ghost;
        next.y++;
        while (fits(game, next)) { ghost = next; next.y++; }
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                if (!pieceCell(ghost.type, ghost.rotation, x, y)) { continue; }
                if (ghost.y + y >= 0) { drawBlock(BOARD_X + (ghost.x + x) * CELL_SIZE, BOARD_Y + (ghost.y + y) * CELL_SIZE, CELL_SIZE, PIECE_COLORS[ghost.type], true); }
                if (game->active.y + y >= 0) { drawBlock(BOARD_X + (game->active.x + x) * CELL_SIZE, BOARD_Y + (game->active.y + y) * CELL_SIZE, CELL_SIZE, PIECE_COLORS[game->active.type], false); }
            }
        }
    }
    drawNeurosCorners((Rectangle){BOARD_X - 4, BOARD_Y - 4, BOARD_WIDTH * CELL_SIZE + 8, BOARD_HEIGHT * CELL_SIZE + 8}, NEON_VIOLET);
    drawNeurosPanel((Rectangle){190, 85, 190, 160}, PANEL, MINT);
    DrawText("HOLD", 207, 101, 20, MINT);
    drawPreview(game->held, 195, 130);
    if (game->holdUsed) { DrawText("USED", 207, 220, 14, MUTED); }
    drawNeurosPanel((Rectangle){820, 85, 190, 160}, PANEL, MINT);
    DrawText("NEXT", 837, 101, 20, MINT);
    drawPreview(game->next, 825, 130);
    drawNeurosPanel((Rectangle){820, 265, 190, 180}, PANEL, MINT);
    DrawText("SCORE", 837, 282, 16, MUTED);
    DrawText(TextFormat("%08d", game->score), 837, 307, 24, TEXT);
    DrawText(TextFormat("LINES  %d", game->lines), 837, 350, 19, TEXT);
    DrawText(TextFormat("LEVEL  %d", game->level), 837, 387, 19, TEXT);
    for (int i = 0; i < 10; i++) {
        DrawRectangle(837 + i * 15, 420, 10, 5, i < game->lines % 10 ? MINT : Fade(MUTED, 0.22f));
    }
    DrawText("CONTROLS", 190, 294, 20, NEON_VIOLET);
    DrawText("LEFT / RIGHT  MOVE", 190, 335, 16, TEXT);
    DrawText("UP / X        ROTATE", 190, 365, 16, TEXT);
    DrawText("Z             ROTATE BACK", 190, 395, 16, TEXT);
    DrawText("DOWN          SOFT DROP", 190, 425, 16, TEXT);
    DrawText("SPACE         HARD DROP", 190, 455, 16, TEXT);
    DrawText("C / SHIFT     HOLD", 190, 485, 16, TEXT);
    DrawText("P             PAUSE", 190, 515, 16, TEXT);
    DrawText("F             TOGGLE FX", 190, 545, 16, TEXT);
    DrawText("M             SOUND", 190, 575, 16, TEXT);
    DrawText("F11           FULLSCREEN", 190, 605, 16, TEXT);
    DrawText("ESC           QUIT", 190, 635, 16, TEXT);
    if (game->paused || game->gameOver) {
        DrawRectangle(BOARD_X, BOARD_Y, BOARD_WIDTH * CELL_SIZE, BOARD_HEIGHT * CELL_SIZE, Fade(BACKGROUND, 0.92f));
        drawCentered(game->gameOver ? "GAME OVER" : "PAUSED", BOARD_X, BOARD_Y + 270, BOARD_WIDTH * CELL_SIZE, 36, game->gameOver ? NEON_VIOLET : MINT);
        drawCentered(game->gameOver ? "PRESS R TO RESTART" : "PRESS P TO RESUME", BOARD_X, BOARD_Y + 325, BOARD_WIDTH * CELL_SIZE, 18, TEXT);
    }
    DrawText("COMPLETE LINES  //  BUILD YOUR SCORE  //  CLEAR THE GRID", 305, 850, 16, MUTED);
}
