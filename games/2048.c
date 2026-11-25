#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <raylib.h>
#include "../shared/neuros_effect.c"
#include "../shared/neuros_audio.c"

#define WIDTH 1200
#define HEIGHT 900
#define GRID 4
#define CELL 142
#define GAP 10
#define BOARD_X 299
#define BOARD_Y 165
#define BOARD_SIZE (GRID * CELL + (GRID + 1) * GAP)

#ifndef FPS
#define FPS 120
#endif

#ifndef VSYNC
#define VSYNC 0
#endif

typedef struct {
    uint64_t cells[GRID][GRID];
    uint64_t score;
    bool reachedGoal;
    bool gameOver;
} Board;

typedef struct {
    Board board;
    Board previous;
    uint64_t best;
    bool canUndo;
    bool showWin;
} Game;

typedef enum {
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_UP,
    MOVE_DOWN
} Direction;

static const Color BACKGROUND = {8, 11, 22, 255};
static const Color PANEL = {20, 28, 44, 255};
static const Color NEON_VIOLET = {169, 92, 252, 255};
static const Color MINT = {52, 236, 168, 255};
static const Color NEON_BLUE = {53, 191, 255, 255};
static const Color TEXT = {229, 241, 245, 255};
static const Color MUTED = {154, 175, 192, 255};

void resetGame(Game *game);
bool spawnTile(Board *board);
bool hasMoves(const Board *board);
bool moveBoard(Board *board, Direction direction);
void playMove(Game *game, Direction direction);
void undoMove(Game *game);
void drawGame(const Game *game);
void drawCentered(const char *text, int x, int y, int width, int size, Color color);
Color tileColor(uint64_t value);

int main(void)
{
    srand((unsigned int)time(NULL));
    if (VSYNC) { SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT); }
    else { SetConfigFlags(FLAG_MSAA_4X_HINT); }
    InitWindow(WIDTH, HEIGHT, "NeurOS | 2048");
    if (!IsWindowReady()) { fprintf(stderr, "Could not open the game window.\n"); return(-1); }
    if (!VSYNC) { SetTargetFPS((FPS > 0 && FPS <= 300) ? FPS : 120); }
    initNeurosAudio();
    NeurosEffect effect = loadNeurosEffect(WIDTH, HEIGHT, 0.48f, (Vector3){0.66f, 0.36f, 0.99f});
    Game game = {0};
    resetGame(&game);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_M)) { toggleNeurosAudio(); }
        if (IsKeyPressed(KEY_F) && effect.ready) { effect.enabled = !effect.enabled; }
        if (IsKeyPressed(KEY_R)) { resetGame(&game); }
        else if (IsKeyPressed(KEY_U)) { undoMove(&game); }
        else if (game.showWin) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) { game.showWin = false; }
        }
        else if (!game.board.gameOver) {
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { playMove(&game, MOVE_LEFT); }
            else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { playMove(&game, MOVE_RIGHT); }
            else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { playMove(&game, MOVE_UP); }
            else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { playMove(&game, MOVE_DOWN); }
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
    uint64_t best = game->best;
    *game = (Game){0};
    game->best = best;
    spawnTile(&game->board);
    spawnTile(&game->board);
}

bool spawnTile(Board *board)
{
    int empty[GRID * GRID];
    int count = 0;
    for (int y = 0; y < GRID; y++) {
        for (int x = 0; x < GRID; x++) {
            if (board->cells[y][x] == 0) { empty[count++] = y * GRID + x; }
        }
    }
    if (count == 0) { return(false); }
    int index = empty[rand() % count];
    board->cells[index / GRID][index % GRID] = rand() % 10 == 0 ? 4 : 2;
    return(true);
}

bool hasMoves(const Board *board)
{
    for (int y = 0; y < GRID; y++) {
        for (int x = 0; x < GRID; x++) {
            uint64_t value = board->cells[y][x];
            if (value == 0) { return(true); }
            if (value > UINT64_MAX / 2) { continue; }
            if (x + 1 < GRID && value == board->cells[y][x + 1]) { return(true); }
            if (y + 1 < GRID && value == board->cells[y + 1][x]) { return(true); }
        }
    }
    return(false);
}

bool moveBoard(Board *board, Direction direction)
{
    bool changed = false;
    for (int line = 0; line < GRID; line++) {
        uint64_t original[GRID] = {0};
        uint64_t compact[GRID] = {0};
        uint64_t result[GRID] = {0};
        int count = 0;
        for (int i = 0; i < GRID; i++) {
            int x = direction == MOVE_LEFT ? i : direction == MOVE_RIGHT ? GRID - 1 - i : line;
            int y = direction == MOVE_UP ? i : direction == MOVE_DOWN ? GRID - 1 - i : line;
            original[i] = board->cells[y][x];
            if (original[i] != 0) { compact[count++] = original[i]; }
        }
        int output = 0;
        for (int i = 0; i < count; i++) {
            uint64_t value = compact[i];
            if (i + 1 < count && value == compact[i + 1] && value <= UINT64_MAX / 2) {
                value *= 2;
                board->score = UINT64_MAX - board->score < value ? UINT64_MAX : board->score + value;
                i++;
            }
            result[output++] = value;
        }
        for (int i = 0; i < GRID; i++) {
            int x = direction == MOVE_LEFT ? i : direction == MOVE_RIGHT ? GRID - 1 - i : line;
            int y = direction == MOVE_UP ? i : direction == MOVE_DOWN ? GRID - 1 - i : line;
            if (original[i] != result[i]) { changed = true; }
            board->cells[y][x] = result[i];
        }
    }
    return(changed);
}

void playMove(Game *game, Direction direction)
{
    Board before = game->board;
    if (!moveBoard(&game->board, direction)) { return; }
    game->previous = before;
    game->canUndo = true;
    bool merged = game->board.score != before.score;
    if (game->board.score > game->best) { game->best = game->board.score; }
    spawnTile(&game->board);
    game->board.gameOver = !hasMoves(&game->board);
    if (!game->board.reachedGoal) {
        for (int y = 0; y < GRID; y++) {
            for (int x = 0; x < GRID; x++) {
                if (game->board.cells[y][x] >= 2048) { game->board.reachedGoal = true; }
            }
        }
        if (game->board.reachedGoal) { game->showWin = true; }
    }
    if (game->showWin) { playNeurosSound(SOUND_WIN); }
    else if (game->board.gameOver) { playNeurosSound(SOUND_LOSE); }
    else if (merged) { playNeurosSound(SOUND_SCORE); }
    else { playNeurosSound(SOUND_MOVE); }
}

void undoMove(Game *game)
{
    if (!game->canUndo) { return; }
    game->board = game->previous;
    game->canUndo = false;
    game->showWin = false;
    playNeurosSound(SOUND_ROTATE);
}

Color tileColor(uint64_t value)
{
    if (value <= 2) { return((Color){88, 152, 255, 255}); }
    if (value <= 4) { return(NEON_BLUE); }
    if (value <= 8) { return((Color){68, 216, 209, 255}); }
    if (value <= 16) { return(MINT); }
    if (value <= 32) { return((Color){125, 230, 120, 255}); }
    if (value <= 64) { return((Color){196, 217, 91, 255}); }
    if (value <= 128) { return((Color){255, 188, 85, 255}); }
    if (value <= 256) { return((Color){255, 130, 119, 255}); }
    if (value <= 512) { return((Color){241, 104, 197, 255}); }
    if (value <= 1024) { return((Color){205, 112, 255, 255}); }
    return(NEON_VIOLET);
}

void drawCentered(const char *text, int x, int y, int width, int size, Color color)
{
    DrawText(text, x + (width - MeasureText(text, size)) / 2, y, size, color);
}

void drawGame(const Game *game)
{
    ClearBackground(BACKGROUND);
    DrawRectangle(20, 18, WIDTH - 40, 52, PANEL);
    DrawText("NEUROS // 2048", 38, 34, 20, MINT);
    DrawText("MERGE PROTOCOL", 960, 36, 16, MUTED);
    DrawText("2048", BOARD_X, 92, 44, TEXT);
    DrawText("COMBINE. UPGRADE. REPEAT.", BOARD_X + 155, 113, 15, MUTED);
    DrawRectangle(45, 165, 215, 115, PANEL);
    DrawText("SCORE", 62, 183, 16, MUTED);
    DrawText(TextFormat("%" PRIu64, game->board.score), 62, 215, 27, TEXT);
    DrawRectangle(45, 296, 215, 115, PANEL);
    DrawText("SESSION BEST", 62, 314, 16, MUTED);
    DrawText(TextFormat("%" PRIu64, game->best), 62, 346, 27, TEXT);
    DrawRectangle(BOARD_X, BOARD_Y, BOARD_SIZE, BOARD_SIZE, PANEL);
    DrawRectangleLinesEx((Rectangle){BOARD_X, BOARD_Y, BOARD_SIZE, BOARD_SIZE}, 2.0f, NEON_VIOLET);
    for (int y = 0; y < GRID; y++) {
        for (int x = 0; x < GRID; x++) {
            int px = BOARD_X + GAP + x * (CELL + GAP);
            int py = BOARD_Y + GAP + y * (CELL + GAP);
            uint64_t value = game->board.cells[y][x];
            if (value == 0) { DrawRectangle(px, py, CELL, CELL, (Color){29, 40, 57, 255}); continue; }
            Color color = tileColor(value);
            DrawRectangle(px, py, CELL, CELL, Fade(color, 0.18f));
            DrawRectangleLinesEx((Rectangle){px, py, CELL, CELL}, 2.0f, color);
            DrawRectangle(px + 8, py + 8, CELL - 16, 3, Fade(color, 0.7f));
            const char *label = TextFormat("%" PRIu64, value);
            int fontSize = 46;
            while (fontSize > 12 && MeasureText(label, fontSize) > CELL - 12) { fontSize -= 2; }
            drawCentered(label, px, py + (CELL - fontSize) / 2, CELL, fontSize, TEXT);
        }
    }
    DrawRectangle(945, 165, 210, 290, PANEL);
    DrawText("CONTROLS", 962, 184, 19, MINT);
    DrawText("ARROWS / WASD", 962, 225, 16, TEXT);
    DrawText("MOVE TILES", 962, 247, 14, MUTED);
    DrawText("U    UNDO", 962, 286, 16, TEXT);
    DrawText("R    RESTART", 962, 315, 16, TEXT);
    DrawText("F    SCREEN FX", 962, 344, 16, TEXT);
    DrawText("M    SOUND", 962, 373, 16, TEXT);
    DrawText("ESC  QUIT", 962, 402, 16, TEXT);
    if (neurosAudio.muted || !neurosAudio.ready) { DrawText("SOUND OFF", 962, 428, 14, MUTED); }
    DrawText("EQUAL TILES MERGE ONCE PER MOVE", BOARD_X + 35, 825, 17, MUTED);
    if (game->showWin || game->board.gameOver) {
        DrawRectangle(BOARD_X, BOARD_Y, BOARD_SIZE, BOARD_SIZE, Fade(BACKGROUND, 0.94f));
        if (game->showWin) {
            drawCentered("2048 REACHED", BOARD_X, BOARD_Y + 215, BOARD_SIZE, 42, MINT);
            drawCentered("ENTER / SPACE TO KEEP PLAYING", BOARD_X, BOARD_Y + 285, BOARD_SIZE, 19, TEXT);
        }
        else {
            drawCentered("NO MOVES LEFT", BOARD_X, BOARD_Y + 215, BOARD_SIZE, 42, NEON_VIOLET);
            drawCentered("U TO UNDO  //  R TO RESTART", BOARD_X, BOARD_Y + 285, BOARD_SIZE, 19, TEXT);
        }
    }
}
