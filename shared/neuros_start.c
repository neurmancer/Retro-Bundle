#include <math.h>
#include <raylib.h>

typedef enum {
    ARCADE_PONG,
    ARCADE_BREAKOUT,
    ARCADE_TETRIS,
    ARCADE_2048,
    ARCADE_GALAGA,
    ARCADE_BLACKJACK
} ArcadeGame;

void arcadeCenter(const char *text, int y, int size, Color color)
{
    DrawText(text, (1200 - MeasureText(text, size)) / 2, y, size, color);
}

void drawArcadeStart(ArcadeGame game)
{
    static const char *const titles[] = {"PONG", "BREAKOUT", "TETRIS", "2048", "GALAGA", "BLACKJACK"};
    static const char *const taglines[] = {
        "TWO PLAYERS. ONE BALL. FIRST TO SEVEN.",
        "BREAK THE GRID. KEEP THE BALL ALIVE.",
        "STACK THE BLOCKS. CLEAR THE LINES.",
        "SLIDE. MERGE. REACH 2048.",
        "DEFEND THE GRID. CLEAR THE ALIEN FLEET.",
        "BEAT THE DEALER. STAY UNDER 22."
    };
    static const char *const controls[] = {
        "P1 W / S     P2 UP / DOWN     P PAUSE",
        "A / D OR ARROWS MOVE     SPACE LAUNCH     P PAUSE",
        "ARROWS MOVE     UP / X ROTATE     SPACE DROP     C HOLD",
        "ARROWS / WASD MOVE     U UNDO     R RESTART",
        "ARROWS / WASD MOVE     SPACE FIRE     P PAUSE",
        "ARROWS BET   ENTER DEAL   H HIT   S STAND   D DOUBLE   P SPLIT   F1 RULES"
    };
    const Color bg = {8, 11, 22, 255};
    const Color mint = {52, 236, 168, 255};
    const Color purple = {169, 92, 252, 255};
    const Color blue = {53, 191, 255, 255};
    const Color text = {229, 241, 245, 255};
    const Color muted = {154, 175, 192, 255};
    float pulse = 0.65f + 0.35f * sinf((float)GetTime() * 3.0f);
    ClearBackground(bg);
    for (int i = 0; i < 75; i++) {
        int x = (i * 197 + 29) % 1200;
        int y = 80 + (i * 173 + 13) % 740;
        DrawRectangle(x, y, i % 5 == 0 ? 3 : 2, i % 5 == 0 ? 3 : 2, Fade(i % 3 == 0 ? purple : blue, 0.45f));
    }
    DrawLine(75, 85, 1125, 85, Fade(purple, 0.7f));
    DrawText("NEUROS // RETRO BUNDLE", 84, 51, 18, mint);
    DrawText("ARCADE SYSTEM", 952, 51, 16, muted);
    arcadeCenter(titles[game], 140, 76, text);
    arcadeCenter(taglines[game], 239, 20, mint);
    DrawRectangleLinesEx((Rectangle){390, 300, 420, 255}, 2.0f, Fade(purple, 0.65f));
    if (game == ARCADE_PONG) {
        for (int y = 322; y < 535; y += 30) { DrawRectangle(598, y, 4, 15, Fade(muted, 0.6f)); }
        DrawRectangle(445, 355, 12, 100, mint);
        DrawRectangle(743, 395, 12, 100, purple);
        DrawRectangle(555 + (int)(sinf((float)GetTime() * 1.5f) * 55), 420, 16, 16, text);
    }
    else if (game == ARCADE_BREAKOUT) {
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 6; col++) {
                Color color = row == 0 ? purple : row == 1 ? blue : mint;
                DrawRectangle(430 + col * 58, 330 + row * 25, 52, 17, Fade(color, 0.3f));
                DrawRectangleLines(430 + col * 58, 330 + row * 25, 52, 17, color);
            }
        }
        DrawCircle(615, 449, 9, text);
        DrawRectangle(534, 510, 132, 12, mint);
    }
    else if (game == ARCADE_TETRIS) {
        static const int blocks[5][6] = {{0, 0, 1, 1, 0, 0}, {0, 0, 0, 1, 1, 0}, {0, 2, 2, 2, 0, 0}, {0, 0, 2, 3, 3, 0}, {3, 3, 3, 3, 0, 0}};
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 6; x++) {
                if (blocks[y][x] == 0) { continue; }
                Color color = blocks[y][x] == 1 ? mint : blocks[y][x] == 2 ? purple : blue;
                DrawRectangle(498 + x * 34, 338 + y * 34, 30, 30, Fade(color, 0.35f));
                DrawRectangleLines(498 + x * 34, 338 + y * 34, 30, 30, color);
            }
        }
    }
    else if (game == ARCADE_2048) {
        static const char *const values[] = {"2", "4", "8", "16", "32", "64", "128", "2048", "4"};
        for (int i = 0; i < 9; i++) {
            int x = 498 + (i % 3) * 70;
            int y = 326 + (i / 3) * 70;
            Color color = i % 3 == 0 ? blue : i % 3 == 1 ? mint : purple;
            DrawRectangle(x, y, 62, 62, Fade(color, 0.18f));
            DrawRectangleLines(x, y, 62, 62, color);
            DrawText(values[i], x + (62 - MeasureText(values[i], 16)) / 2, y + 23, 16, text);
        }
    }
    else if (game == ARCADE_GALAGA) {
        for (int row = 0; row < 2; row++) {
            for (int col = 0; col < 5; col++) {
                int x = 485 + col * 50;
                int y = 345 + row * 42;
                Color color = row == 0 ? purple : blue;
                DrawRectangle(x + 5, y, 20, 5, color);
                DrawRectangle(x, y + 5, 30, 15, color);
                DrawRectangle(x + 5, y + 20, 5, 5, color);
                DrawRectangle(x + 20, y + 20, 5, 5, color);
                DrawRectangle(x + 5, y + 10, 5, 5, bg);
                DrawRectangle(x + 20, y + 10, 5, 5, bg);
            }
        }
        DrawRectangle(594, 465, 12, 45, mint);
        DrawRectangle(568, 488, 64, 15, mint);
        DrawRectangle(596, 435, 8, 18, text);
    }
    arcadeCenter("PRESS ENTER TO START", 610, 28, Fade(text, pulse));
    arcadeCenter(controls[game], 690, 18, muted);
    arcadeCenter("F11 FULLSCREEN    F EFFECTS    M SOUND    ESC QUIT", 730, 16, muted);
}
