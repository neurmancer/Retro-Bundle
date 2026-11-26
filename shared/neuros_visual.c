#include <raylib.h>

void drawNeurosBackdrop(int width, int height, Color base, Color accent)
{
    ClearBackground(base);
    DrawRectangle(14, 14, width - 28, height - 28, Fade(accent, 0.16f));
    DrawRectangle(18, 18, width - 36, height - 36, base);
}

void drawNeurosPanel(Rectangle rect, Color fill, Color accent)
{
    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, 3.0f, accent);
    DrawRectangle((int)rect.x + 6, (int)rect.y + 6, (int)rect.width - 12, 3, Fade(accent, 0.35f));
}

void drawNeurosCorners(Rectangle rect, Color color)
{
    DrawRectangleLinesEx(rect, 4.0f, color);
}

void drawNeurosOrb(Vector2 position, float radius, Color color)
{
    int size = (int)(radius * 2.0f);
    DrawRectangle((int)(position.x - radius), (int)(position.y - radius), size, size, color);
}

void drawNeurosDigit(int digit, int x, int y, int scale, Color color)
{
    static const unsigned char segments[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    unsigned char bits = segments[digit % 10];
    if (bits & 1) { DrawRectangle(x + scale, y, scale * 3, scale, color); }
    if (bits & 2) { DrawRectangle(x + scale * 4, y + scale, scale, scale * 3, color); }
    if (bits & 4) { DrawRectangle(x + scale * 4, y + scale * 5, scale, scale * 3, color); }
    if (bits & 8) { DrawRectangle(x + scale, y + scale * 8, scale * 3, scale, color); }
    if (bits & 16) { DrawRectangle(x, y + scale * 5, scale, scale * 3, color); }
    if (bits & 32) { DrawRectangle(x, y + scale, scale, scale * 3, color); }
    if (bits & 64) { DrawRectangle(x + scale, y + scale * 4, scale * 3, scale, color); }
}

void drawRetroStart(int game)
{
    static const char *const titles[] = {"PONG", "BREAKOUT", "TETRIS", "2048"};
    static const char *const instructions[] = {
        "P1 W / S     P2 UP / DOWN     FIRST TO 7",
        "ARROWS / A D MOVE     SPACE LAUNCH",
        "ARROWS MOVE     UP ROTATE     SPACE DROP     C HOLD",
        "ARROWS / WASD MOVE     U UNDO     R RESTART"
    };
    Color bg = {8, 11, 22, 255};
    Color mint = {83, 240, 177, 255};
    Color violet = {179, 112, 255, 255};
    Color blue = {84, 195, 255, 255};
    Color white = {232, 245, 237, 255};
    drawNeurosBackdrop(1200, 900, bg, violet);
    DrawText("NEUROS  /  ARCADE SYSTEM", 56, 48, 20, mint);
    DrawText("1 UP", 80, 110, 22, white);
    DrawText("HIGH SCORE", 515, 110, 22, white);
    DrawText("000000", 556, 142, 22, violet);
    int titleSize = 82;
    DrawText(titles[game], (1200 - MeasureText(titles[game], titleSize)) / 2, 220, titleSize, white);
    DrawRectangle(365, 344, 470, 215, violet);
    DrawRectangle(370, 349, 460, 205, bg);
    if (game == 0) {
        for (int y = 365; y < 540; y += 26) { DrawRectangle(597, y, 6, 14, white); }
        DrawRectangle(408, 390, 14, 84, mint);
        DrawRectangle(778, 419, 14, 84, violet);
        DrawRectangle(555, 445, 18, 18, white);
    }
    else if (game == 1) {
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 8; x++) { DrawRectangle(403 + x * 50, 375 + y * 26, 44, 19, y == 0 ? violet : y == 1 ? blue : mint); }
        }
        DrawRectangle(590, 466, 16, 16, white);
        DrawRectangle(537, 515, 126, 12, mint);
    }
    else if (game == 2) {
        static const int blocks[4][6] = {{0, 0, 1, 1, 0, 0}, {0, 0, 0, 1, 1, 0}, {0, 2, 2, 2, 0, 0}, {3, 3, 2, 3, 3, 0}};
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 6; x++) {
                int cell = blocks[y][x];
                if (cell) { DrawRectangle(497 + x * 35, 382 + y * 35, 32, 32, cell == 1 ? mint : cell == 2 ? violet : blue); }
            }
        }
    }
    else {
        static const char *const values[] = {"2", "4", "8", "16", "32", "64", "128", "256", "2048"};
        for (int i = 0; i < 9; i++) {
            int x = 501 + i % 3 * 68, y = 354 + i / 3 * 64;
            DrawRectangle(x, y, 62, 58, i % 3 == 0 ? blue : i % 3 == 1 ? mint : violet);
            DrawText(values[i], x + (62 - MeasureText(values[i], 18)) / 2, y + 20, 18, bg);
        }
    }
    const char *start = "PRESS ENTER TO START";
    if ((int)(GetTime() * 2.0) % 2 == 0) { DrawText(start, (1200 - MeasureText(start, 28)) / 2, 615, 28, mint); }
    DrawText(instructions[game], (1200 - MeasureText(instructions[game], 18)) / 2, 700, 18, white);
    const char *options = "F11 FULLSCREEN    F SCREEN FX    M SOUND    ESC QUIT";
    DrawText(options, (1200 - MeasureText(options, 16)) / 2, 742, 16, violet);
    DrawText("CREDIT 01", 55, 838, 20, white);
    DrawText("NEUROS", 1054, 838, 20, white);
}
