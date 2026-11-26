#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <sys/random.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <raylib.h>
#include "../../shared/neuros_effect.c"
#include "../../shared/neuros_start.c"
#include "../../shared/neuros_audio.c"

#define WIDTH 1200
#define HEIGHT 900
#define DECK_SIZE 52
#define HAND_SIZE 52
#define STARTING_CHIPS 1000
#define MIN_BET 10
#define MAX_BET 10000
#define BET_STEP 10

#ifndef FPS
#define FPS 120
#endif

#ifndef VSYNC
#define VSYNC 0
#endif

typedef struct {
    int rank;
    int suit;
} Card;

typedef struct {
    Card cards[HAND_SIZE];
    int count;
} Hand;

typedef enum {
    BETTING,
    INSURANCE,
    PLAYER_TURN,
    DEALER_TURN,
    ROUND_OVER
} Phase;

typedef enum {
    NO_RESULT,
    PLAYER_BLACKJACK,
    PLAYER_WINS,
    DEALER_WINS,
    PUSH,
    PLAYER_BUST,
    PLAYER_SURRENDER
} Result;

typedef struct {
    Card deck[DECK_SIZE];
    int nextCard;
    Hand player;
    Hand splitHand;
    Hand dealer;
    int64_t chips;
    int64_t bet;
    int64_t splitBet;
    int64_t insuranceBet;
    int64_t roundStartChips;
    int64_t roundBaseBet;
    int activeHand;
    bool split;
    bool splitAces;
    bool surrendered;
    Phase phase;
    Result result;
    Result splitResult;
    float dealerTimer;
    bool randomError;
} Game;

static const Color BACKGROUND = {8, 11, 25, 255};
static const Color TABLE = {13, 22, 43, 255};
static const Color CARD = {23, 34, 85, 255};
static const Color CYAN = {63, 215, 255, 255};
static const Color MINT = {52, 236, 168, 255};
static const Color NEON_PURPLE = {169, 92, 252, 255};
static const Color NEON_MAGENTA = {237, 62, 184, 255};
static const Color TEXT = {229, 241, 245, 255};
static const Color MUTED = {154, 175, 192, 255};
static Texture2D cardFaceArtwork = {0};
static Texture2D cardBackArtwork = {0};
static Texture2D suitArtwork[4] = {0};
static Texture2D courtArtwork[2][3] = {0};
static bool courtColored[2][3] = {0};
static Shader suitShader = {0};
static int suitPaletteLocation = -1;
static Shader cardLightShader = {0};
static Texture2D cardLightTexture = {0};
static bool cardLightReady = false;
static bool cardLightEnabled = true;
static RenderTexture2D cardFaces[4][13] = {0};
static RenderTexture2D cardBack = {0};
static bool buildingCardArt = false;
static Font casinoFont = {0};
static Font rankFont = {0};
static float playerCardAge[HAND_SIZE] = {0};
static float dealerCardAge[HAND_SIZE] = {0};
static float splitCardAge[HAND_SIZE] = {0};
static int lastPlayerCount = 0;
static int lastDealerCount = 0;
static int lastSplitCount = 0;

typedef bool (*RandomWord)(uint32_t *value, void *context);
bool kernelRandomWord(uint32_t *value, void *context);
bool randomBelow(uint32_t bound, RandomWord source, void *context, uint32_t *result);
bool shuffleWithRandom(Game *game, RandomWord source, void *context);
bool resetGame(Game *game);
bool shuffleDeck(Game *game);
int handValue(const Hand *hand);
bool isBlackjack(const Hand *hand);
void changeBet(Game *game, int change);
void dealRound(Game *game);
void hit(Game *game);
void stand(Game *game);
void doubleDown(Game *game);
void splitPair(Game *game);
void surrender(Game *game);
void insuranceChoice(Game *game, bool accept);
void resolvePeek(Game *game);
void advanceHand(Game *game);
Hand *activePlayerHand(Game *game);
const Hand *currentPlayerHand(const Game *game);
void dealerStep(Game *game, float dt);
void finishRound(Game *game);
void nextRound(Game *game);
void drawGame(const Game *game);
void drawCard(Card card, float x, float y, bool hidden);
Texture2D loadCardTexture(const char *name);
Shader loadBlackjackShader(const char *name);
void initCardLight(void);
void unloadCardLight(void);
void drawCardHalo(Vector2 center, Color color);
void initCardArt(void);
void unloadCardArt(void);
void drawSuit(int suit, Vector2 center, float size, Color color);
void drawHandArea(const Hand *hand, float y, bool hideSecond, float center, float maxSpan, const float *ages);
void drawCenter(const char *text, int y, int size, Color color);
void loadCasinoFonts(void);
void unloadCasinoFonts(void);
void uiDraw(const char *text, int x, int y, int size, Color color);
void cardDraw(const char *text, int x, int y, int size, Color color);
int uiMeasure(const char *text, int size);
int cardMeasure(const char *text, int size);
bool pipPosition(int rank, int index, Vector2 *position);
void updateCardAnimation(const Game *game, float dt);
Vector2 blackjackWindowSize(int monitorWidth, int monitorHeight, float dpiX, float dpiY);
void fitBlackjackWindow(void);
Vector2 blackjackPointer(Vector2 position, int screenWidth, int screenHeight);
Rectangle blackjackActionRect(int index);
void drawTableButton(Rectangle rect, const char *label, Color color, bool enabled, bool pressed);

Font loadCasinoFont(const char *name)
{
    char local[256];
    char executable[1024];
    char parent[1024];
    snprintf(local, sizeof(local), "games/blakjack/assetts/fonts/%s", name);
    snprintf(executable, sizeof(executable), "%s%s", GetApplicationDirectory(), local);
    snprintf(parent, sizeof(parent), "%s../%s", GetApplicationDirectory(), local);
    const char *path = NULL;
    if (FileExists(local)) { path = local; }
    else if (FileExists(executable)) { path = executable; }
    else if (FileExists(parent)) { path = parent; }
    if (path == NULL) { return((Font){0}); }
    Font font = LoadFontEx(path, 64, NULL, 95);
    if (font.texture.id != 0) { SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR); }
    return(font);
}

void loadCasinoFonts(void)
{
    casinoFont = loadCasinoFont("Rajdhani-SemiBold.ttf");
    rankFont = loadCasinoFont("Rajdhani-Bold.ttf");
}

void unloadCasinoFonts(void)
{
    if (casinoFont.texture.id != 0 && casinoFont.texture.id != GetFontDefault().texture.id) { UnloadFont(casinoFont); }
    if (rankFont.texture.id != 0 && rankFont.texture.id != GetFontDefault().texture.id) { UnloadFont(rankFont); }
}

void uiDraw(const char *text, int x, int y, int size, Color color)
{
    Font font = casinoFont.texture.id != 0 ? casinoFont : GetFontDefault();
    DrawTextEx(font, text, (Vector2){(float)x, (float)y}, (float)size, 0.7f, color);
}

void cardDraw(const char *text, int x, int y, int size, Color color)
{
    Font font = rankFont.texture.id != 0 ? rankFont : GetFontDefault();
    DrawTextEx(font, text, (Vector2){(float)x, (float)y}, (float)size, 0.3f, color);
}

int uiMeasure(const char *text, int size)
{
    Font font = casinoFont.texture.id != 0 ? casinoFont : GetFontDefault();
    return((int)MeasureTextEx(font, text, (float)size, 0.7f).x);
}

int cardMeasure(const char *text, int size)
{
    Font font = rankFont.texture.id != 0 ? rankFont : GetFontDefault();
    return((int)MeasureTextEx(font, text, (float)size, 0.3f).x);
}

bool pipPosition(int rank, int index, Vector2 *position)
{
    static const signed char coords[9][10][2] = {
        {{0,-2},{0,2}},
        {{0,-2},{0,0},{0,2}},
        {{-1,-2},{1,-2},{-1,2},{1,2}},
        {{-1,-2},{1,-2},{0,0},{-1,2},{1,2}},
        {{-1,-2},{1,-2},{-1,0},{1,0},{-1,2},{1,2}},
        {{-1,-2},{1,-2},{0,-1},{-1,0},{1,0},{-1,2},{1,2}},
        {{-1,-2},{1,-2},{0,-1},{-1,0},{1,0},{0,1},{-1,2},{1,2}},
        {{-1,-2},{1,-2},{-1,-1},{1,-1},{0,0},{-1,1},{1,1},{-1,2},{1,2}},
        {{-1,-2},{1,-2},{-1,-1},{1,-1},{0,-1},{-1,1},{1,1},{0,1},{-1,2},{1,2}}
    };
    if (rank < 2 || rank > 10 || index < 0 || index >= rank || position == NULL) { return(false); }
    *position = (Vector2){coords[rank - 2][index][0] * 19.0f, coords[rank - 2][index][1] * 16.0f};
    return(true);
}

void updateCardAnimation(const Game *game, float dt)
{
    if (game->player.count < lastPlayerCount) { lastPlayerCount = 0; }
    if (game->dealer.count < lastDealerCount) { lastDealerCount = 0; }
    if (game->splitHand.count < lastSplitCount) { lastSplitCount = 0; }
    for (int i = 0; i < game->player.count; i++) {
        if (i >= lastPlayerCount) { playerCardAge[i] = 0.0f; }
        playerCardAge[i] += dt;
    }
    for (int i = 0; i < game->dealer.count; i++) {
        if (i >= lastDealerCount) { dealerCardAge[i] = 0.0f; }
        dealerCardAge[i] += dt;
    }
    for (int i = 0; i < game->splitHand.count; i++) {
        if (i >= lastSplitCount) { splitCardAge[i] = 0.0f; }
        splitCardAge[i] += dt;
    }
    lastPlayerCount = game->player.count;
    lastDealerCount = game->dealer.count;
    lastSplitCount = game->splitHand.count;
}

Vector2 blackjackWindowSize(int monitorWidth, int monitorHeight, float dpiX, float dpiY)
{
    if (monitorWidth <= 0 || monitorHeight <= 0) { return((Vector2){WIDTH, HEIGHT}); }
    if (!isfinite(dpiX) || dpiX < 1.0f) { dpiX = 1.0f; }
    if (!isfinite(dpiY) || dpiY < 1.0f) { dpiY = 1.0f; }
    float maxWidth = (float)monitorWidth / dpiX * 0.90f;
    float maxHeight = (float)monitorHeight / dpiY * 0.90f;
    float scale = fminf(1.0f, fminf(maxWidth / WIDTH, maxHeight / HEIGHT));
    return((Vector2){floorf(WIDTH * scale), floorf(HEIGHT * scale)});
}

void fitBlackjackWindow(void)
{
    int monitor = GetCurrentMonitor();
    Vector2 dpi = GetWindowScaleDPI();
    if (GetScreenWidth() > 0 && GetScreenHeight() > 0) {
        dpi.x = fmaxf(dpi.x, (float)GetRenderWidth() / GetScreenWidth());
        dpi.y = fmaxf(dpi.y, (float)GetRenderHeight() / GetScreenHeight());
    }
    Vector2 size = blackjackWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor), dpi.x, dpi.y);
    if (size.x > 0 && size.y > 0 && (size.x != WIDTH || size.y != HEIGHT)) { SetWindowSize((int)size.x, (int)size.y); }
}

Vector2 blackjackPointer(Vector2 position, int screenWidth, int screenHeight)
{
    Rectangle viewport = neurosViewport(WIDTH, HEIGHT, screenWidth, screenHeight);
    if (viewport.width <= 0.0f || viewport.height <= 0.0f || !CheckCollisionPointRec(position, viewport)) { return((Vector2){-1.0f, -1.0f}); }
    return((Vector2){(position.x - viewport.x) * WIDTH / viewport.width, (position.y - viewport.y) * HEIGHT / viewport.height});
}

Rectangle blackjackActionRect(int index)
{
    return((Rectangle){223.0f + index * 152.0f, 355.0f, 140.0f, 40.0f});
}

void drawTableButton(Rectangle rect, const char *label, Color color, bool enabled, bool pressed)
{
    Color ink = enabled ? color : Fade(MUTED, 0.35f);
    DrawRectangleRounded(rect, 0.15f, 4, Fade(ink, pressed ? 0.35f : 0.08f));
    DrawRectangleRoundedLinesEx(rect, 0.15f, 4, 1.0f, Fade(ink, 0.65f));
    uiDraw(label, (int)(rect.x + (rect.width - uiMeasure(label, 19)) / 2), (int)rect.y + 8, 19, ink);
}

int main(void)
{
    if (VSYNC) { SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE); }
    else { SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE); }
    InitWindow(WIDTH, HEIGHT, "NeurOS | Blackjack");
    if (!IsWindowReady()) { fprintf(stderr, "Could not open the game window.\n"); return(-1); }
    fitBlackjackWindow();
    if (!VSYNC) { SetTargetFPS((FPS > 0 && FPS <= 300) ? FPS : 120); }
    initNeurosAudio();
    NeurosEffect effect = loadNeurosEffectNamed(WIDTH, HEIGHT, 0.65f, (Vector3){0.66f, 0.36f, 0.99f}, "blackjack_glow.fs");
    if (!setNeurosRenderScale(&effect, 2) && effect.target.id != 0) { SetTextureFilter(effect.target.texture, TEXTURE_FILTER_BILINEAR); }
    loadCasinoFonts();
    initCardArt();
    initCardLight();
    Game game = {0};
    if (!resetGame(&game)) {
        fprintf(stderr, "Could not obtain kernel randomness for the deck.\n");
        unloadCardLight();
        unloadCardArt();
        unloadCasinoFonts();
        unloadNeurosAudio();
        unloadNeurosEffect(&effect);
        CloseWindow();
        return(-1);
    }
    bool showTitle = true;
    bool showHelp = false;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) { toggleNeurosFullscreen(&effect); }
        if (IsKeyPressed(KEY_M)) { toggleNeurosAudio(); }
        if (IsKeyPressed(KEY_F)) {
            if (effect.ready) { effect.enabled = !effect.enabled; }
            cardLightEnabled = !cardLightEnabled;
        }
        if (showTitle) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { showTitle = false; }
            beginNeurosEffect(&effect);
            drawArcadeStart(ARCADE_BLACKJACK);
            DrawRectangle(300, 130, 600, 100, BACKGROUND);
            const char *title = "BLACKJACK";
            cardDraw(title, (WIDTH - cardMeasure(title, 60)) / 2, 148, 60, TEXT);
            if (cardLightReady && cardLightEnabled) {
                BeginBlendMode(BLEND_ADDITIVE);
                BeginShaderMode(cardLightShader);
                drawCardHalo((Vector2){540.0f, 426.0f}, CYAN);
                drawCardHalo((Vector2){665.0f, 426.0f}, NEON_MAGENTA);
                EndShaderMode();
                EndBlendMode();
            }
            drawCard((Card){1, 2}, 480.0f, 342.0f, false);
            drawCard((Card){1, 0}, 605.0f, 342.0f, true);
            endNeurosEffect(&effect);
            continue;
        }
        if (IsKeyPressed(KEY_F1)) { showHelp = !showHelp; }
        if (showHelp) {
            beginNeurosEffect(&effect);
            drawGame(&game);
            DrawRectangle(120, 115, 960, 650, Fade(BACKGROUND, 0.97f));
            DrawRectangleLinesEx((Rectangle){120, 115, 960, 650}, 2.0f, NEON_PURPLE);
            drawCenter("TABLE RULES", 145, 36, MINT);
            drawCenter("SINGLE DECK  //  DEALER STANDS ON SOFT 17", 215, 25, TEXT);
            drawCenter("BET LIMIT 10 - 10000", 565, 22, MUTED);
            drawCenter("RESHUFFLE WHEN FEWER THAN 40 CARDS REMAIN", 605, 21, MUTED);
            drawCenter("BLACKJACK PAYS 3:2  //  INSURANCE PAYS 2:1", 260, 25, TEXT);
            drawCenter("SPLIT MATCHING RANKS ONCE; NO RESPLITS", 305, 25, TEXT);
            drawCenter("SPLIT ACES RECEIVE ONE CARD EACH", 350, 25, TEXT);
            drawCenter("DOUBLE ON FIRST TWO CARDS, INCLUDING AFTER SPLIT", 395, 25, TEXT);
            drawCenter("LATE SURRENDER BEFORE SPLITTING", 440, 25, TEXT);
            drawCenter("P SPLIT   R SURRENDER   H HIT   S STAND   D DOUBLE", 520, 23, CYAN);
            drawCenter("F1 TO RETURN", 650, 24, MINT);
            endNeurosEffect(&effect);
            continue;
        }
        Vector2 pointer = blackjackPointer(GetMousePosition(), GetScreenWidth(), GetScreenHeight());
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (game.phase == BETTING) {
            int step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? BET_STEP * 10 : BET_STEP;
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_DOWN) || (clicked && CheckCollisionPointRec(pointer, (Rectangle){380, 305, 120, 40}))) { changeBet(&game, -step); }
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_UP) || (clicked && CheckCollisionPointRec(pointer, (Rectangle){700, 305, 120, 40}))) { changeBet(&game, step); }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || (clicked && CheckCollisionPointRec(pointer, (Rectangle){510, 305, 180, 40}))) { dealRound(&game); }
        }
        else if (game.phase == INSURANCE) {
            if (IsKeyPressed(KEY_I) || (clicked && CheckCollisionPointRec(pointer, (Rectangle){400, 390, 190, 40}))) { insuranceChoice(&game, true); }
            else if (IsKeyPressed(KEY_N) || (clicked && CheckCollisionPointRec(pointer, (Rectangle){610, 390, 190, 40}))) { insuranceChoice(&game, false); }
        }
        else if (game.phase == PLAYER_TURN) {
            if (IsKeyPressed(KEY_H) || (clicked && CheckCollisionPointRec(pointer, blackjackActionRect(0)))) { hit(&game); }
            else if (IsKeyPressed(KEY_S) || (clicked && CheckCollisionPointRec(pointer, blackjackActionRect(1)))) { stand(&game); }
            else if (IsKeyPressed(KEY_D) || (clicked && CheckCollisionPointRec(pointer, blackjackActionRect(2)))) { doubleDown(&game); }
            else if (IsKeyPressed(KEY_P) || (clicked && CheckCollisionPointRec(pointer, blackjackActionRect(3)))) { splitPair(&game); }
            else if (IsKeyPressed(KEY_R) || (clicked && CheckCollisionPointRec(pointer, blackjackActionRect(4)))) { surrender(&game); }
        }
        else if (game.phase == DEALER_TURN) {
            float dt = GetFrameTime();
            if (dt > 0.1f) { dt = 0.1f; }
            dealerStep(&game, dt);
        }
        else if (game.phase == ROUND_OVER) {
            if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || (clicked && CheckCollisionPointRec(pointer, (Rectangle){470, 421, 260, 28}))) && game.chips >= MIN_BET) { nextRound(&game); }
            else if (IsKeyPressed(KEY_R) && game.chips < MIN_BET) { if (!resetGame(&game)) { game.randomError = true; } }
        }
        updateCardAnimation(&game, fminf(GetFrameTime(), 0.1f));
        beginNeurosEffect(&effect);
        drawGame(&game);
        endNeurosEffect(&effect);
    }
    unloadCardLight();
    unloadCardArt();
    unloadCasinoFonts();
    unloadNeurosAudio();
    unloadNeurosEffect(&effect);
    CloseWindow();
    return(0);
}

bool kernelRandomWord(uint32_t *value, void *context)
{
    (void)context;
    unsigned char *output = (unsigned char *)value;
    size_t remaining = sizeof(*value);
    while (remaining > 0) {
        ssize_t received = getrandom(output, remaining, 0);
        if (received < 0 && errno == EINTR) { continue; }
        if (received <= 0) { return(false); }
        output += received;
        remaining -= (size_t)received;
    }
    return(true);
}

bool randomBelow(uint32_t bound, RandomWord source, void *context, uint32_t *result)
{
    if (bound == 0 || source == NULL || result == NULL) { return(false); }
    uint32_t threshold = (uint32_t)(0u - bound) % bound;
    uint32_t value = 0;
    do {
        if (!source(&value, context)) { return(false); }
    } while (value < threshold);
    *result = value % bound;
    return(true);
}

bool shuffleWithRandom(Game *game, RandomWord source, void *context)
{
    if (game == NULL || source == NULL) { return(false); }
    Card deck[DECK_SIZE];
    for (int i = 0; i < DECK_SIZE; i++) { deck[i] = (Card){i % 13 + 1, i / 13}; }
    for (int i = DECK_SIZE - 1; i > 0; i--) {
        uint32_t j = 0;
        if (!randomBelow((uint32_t)i + 1, source, context, &j)) { return(false); }
        Card temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
    for (int i = 0; i < DECK_SIZE; i++) { game->deck[i] = deck[i]; }
    game->nextCard = 0;
    return(true);
}

bool shuffleDeck(Game *game)
{
    return(shuffleWithRandom(game, kernelRandomWord, NULL));
}

bool resetGame(Game *game)
{
    Game fresh = {0};
    fresh.chips = STARTING_CHIPS;
    fresh.bet = 50;
    fresh.phase = BETTING;
    if (!shuffleDeck(&fresh)) { return(false); }
    *game = fresh;
    return(true);
}

int handValue(const Hand *hand)
{
    int total = 0;
    int aces = 0;
    for (int i = 0; i < hand->count; i++) {
        int rank = hand->cards[i].rank;
        if (rank == 1) { total += 11; aces++; }
        else if (rank >= 10) { total += 10; }
        else { total += rank; }
    }
    while (total > 21 && aces > 0) { total -= 10; aces--; }
    return(total);
}

bool isBlackjack(const Hand *hand)
{
    return(hand->count == 2 && handValue(hand) == 21);
}

void changeBet(Game *game, int change)
{
    if (game->phase != BETTING) { return; }
    int64_t newBet = game->bet + change;
    if (newBet < MIN_BET) { newBet = MIN_BET; }
    if (newBet > MAX_BET) { newBet = MAX_BET; }
    if (newBet > game->chips) { newBet = game->chips - game->chips % 2; }
    if (newBet != game->bet) { game->bet = newBet; playNeurosSound(SOUND_MOVE); }
}

void dealRound(Game *game)
{
    if (game->phase != BETTING || game->bet < MIN_BET || game->bet > MAX_BET || game->bet > game->chips || game->bet % 2 != 0) { return; }
    if (DECK_SIZE - game->nextCard < 40 && !shuffleDeck(game)) { game->randomError = true; return; }
    game->randomError = false;
    game->player = (Hand){0};
    game->dealer = (Hand){0};
    game->splitHand = (Hand){0};
    game->split = false;
    game->splitAces = false;
    game->surrendered = false;
    game->insuranceBet = 0;
    game->splitBet = 0;
    game->splitResult = NO_RESULT;
    game->activeHand = 0;
    game->roundStartChips = game->chips;
    game->roundBaseBet = game->bet;
    game->chips -= game->bet;
    game->player.cards[game->player.count++] = game->deck[game->nextCard++];
    game->dealer.cards[game->dealer.count++] = game->deck[game->nextCard++];
    game->player.cards[game->player.count++] = game->deck[game->nextCard++];
    game->dealer.cards[game->dealer.count++] = game->deck[game->nextCard++];
    game->result = NO_RESULT;
    game->phase = PLAYER_TURN;
    playNeurosSound(SOUND_ROTATE);
    if (game->dealer.cards[0].rank == 1 && game->chips >= game->bet / 2) { game->phase = INSURANCE; }
    else { resolvePeek(game); }
}

Hand *activePlayerHand(Game *game)
{
    return(game->activeHand == 0 ? &game->player : &game->splitHand);
}

const Hand *currentPlayerHand(const Game *game)
{
    return(game->activeHand == 0 ? &game->player : &game->splitHand);
}

void resolvePeek(Game *game)
{
    if (isBlackjack(&game->dealer)) {
        if (game->insuranceBet > 0) { game->chips += game->insuranceBet * 3; }
        game->phase = PLAYER_TURN;
        finishRound(game);
    }
    else if (isBlackjack(&game->player)) { game->phase = PLAYER_TURN; finishRound(game); }
    else { game->phase = PLAYER_TURN; }
}

void insuranceChoice(Game *game, bool accept)
{
    if (game->phase != INSURANCE) { return; }
    if (accept && game->chips >= game->bet / 2) {
        game->insuranceBet = game->bet / 2;
        game->chips -= game->insuranceBet;
    }
    resolvePeek(game);
}

void advanceHand(Game *game)
{
    if (game->split && game->activeHand == 0 && !game->splitAces) {
        game->activeHand = 1;
        if (handValue(&game->splitHand) < 21) { return; }
    }
    if (handValue(&game->player) > 21 && (!game->split || handValue(&game->splitHand) > 21)) {
        finishRound(game);
        return;
    }
    game->phase = DEALER_TURN;
    game->dealerTimer = 0.55f;
}

void hit(Game *game)
{
    if (game->phase != PLAYER_TURN || game->splitAces) { return; }
    Hand *hand = activePlayerHand(game);
    if (game->nextCard >= DECK_SIZE || hand->count >= HAND_SIZE) { return; }
    hand->cards[hand->count++] = game->deck[game->nextCard++];
    playNeurosSound(SOUND_ROTATE);
    if (handValue(hand) >= 21) { advanceHand(game); }
}

void stand(Game *game)
{
    if (game->phase != PLAYER_TURN) { return; }
    advanceHand(game);
}

void doubleDown(Game *game)
{
    if (game->phase != PLAYER_TURN || game->splitAces) { return; }
    Hand *hand = activePlayerHand(game);
    int64_t *wager = game->activeHand == 0 ? &game->bet : &game->splitBet;
    if (hand->count != 2 || game->chips < *wager || game->nextCard >= DECK_SIZE || hand->count >= HAND_SIZE) { return; }
    game->chips -= *wager;
    *wager *= 2;
    hand->cards[hand->count++] = game->deck[game->nextCard++];
    playNeurosSound(SOUND_ROTATE);
    advanceHand(game);
}

void splitPair(Game *game)
{
    if (game->phase != PLAYER_TURN || game->split || game->player.count != 2 ||
        game->player.cards[0].rank != game->player.cards[1].rank || game->chips < game->bet ||
        game->nextCard + 2 > DECK_SIZE) { return; }
    game->chips -= game->bet;
    game->splitBet = game->bet;
    game->split = true;
    game->splitAces = game->player.cards[0].rank == 1;
    game->splitHand = (Hand){0};
    game->splitHand.cards[game->splitHand.count++] = game->player.cards[1];
    game->player.count = 1;
    game->player.cards[game->player.count++] = game->deck[game->nextCard++];
    game->splitHand.cards[game->splitHand.count++] = game->deck[game->nextCard++];
    playerCardAge[1] = 0.0f;
    lastPlayerCount = 1;
    lastSplitCount = 0;
    playNeurosSound(SOUND_SCORE);
    if (game->splitAces) { advanceHand(game); }
    else if (handValue(&game->player) == 21) { advanceHand(game); }
}

void surrender(Game *game)
{
    if (game->phase != PLAYER_TURN || game->split || game->player.count != 2) { return; }
    game->surrendered = true;
    finishRound(game);
}

void dealerStep(Game *game, float dt)
{
    if (game->phase != DEALER_TURN) { return; }
    game->dealerTimer -= dt;
    if (game->dealerTimer > 0.0f) { return; }
    if (handValue(&game->dealer) < 17 && game->nextCard < DECK_SIZE && game->dealer.count < HAND_SIZE) {
        game->dealer.cards[game->dealer.count++] = game->deck[game->nextCard++];
        game->dealerTimer = 0.55f;
        playNeurosSound(SOUND_ROTATE);
    }
    else { finishRound(game); }
}

Result settleHand(Game *game, const Hand *hand, int64_t wager, bool naturalAllowed)
{
    int total = handValue(hand);
    int dealer = handValue(&game->dealer);
    bool natural = naturalAllowed && isBlackjack(hand);
    bool dealerNatural = isBlackjack(&game->dealer);
    if (total > 21) { return(PLAYER_BUST); }
    if (natural && dealerNatural) { game->chips += wager; return(PUSH); }
    if (natural) { game->chips += wager + wager * 3 / 2; return(PLAYER_BLACKJACK); }
    if (dealerNatural) { return(DEALER_WINS); }
    if (dealer > 21 || total > dealer) { game->chips += wager * 2; return(PLAYER_WINS); }
    if (total == dealer) { game->chips += wager; return(PUSH); }
    return(DEALER_WINS);
}

void finishRound(Game *game)
{
    if (game->phase != PLAYER_TURN && game->phase != DEALER_TURN) { return; }
    if (game->surrendered) { game->chips += game->bet / 2; game->result = PLAYER_SURRENDER; }
    else { game->result = settleHand(game, &game->player, game->bet, !game->split); }
    if (game->split) { game->splitResult = settleHand(game, &game->splitHand, game->splitBet, false); }
    game->phase = ROUND_OVER;
    if (game->result == PLAYER_BLACKJACK || game->result == PLAYER_WINS || game->splitResult == PLAYER_WINS) { playNeurosSound(SOUND_WIN); }
    else if (game->result == PUSH || game->splitResult == PUSH) { playNeurosSound(SOUND_SCORE); }
    else { playNeurosSound(SOUND_LOSE); }
}

void nextRound(Game *game)
{
    if (game->phase != ROUND_OVER || game->chips < MIN_BET) { return; }
    game->bet = game->roundBaseBet;
    if (game->bet > MAX_BET) { game->bet = MAX_BET; }
    if (game->bet > game->chips) { game->bet = game->chips - game->chips % 2; }
    game->player = (Hand){0};
    game->dealer = (Hand){0};
    game->splitHand = (Hand){0};
    game->split = false;
    game->splitAces = false;
    game->insuranceBet = 0;
    game->splitBet = 0;
    game->result = NO_RESULT;
    game->splitResult = NO_RESULT;
    game->phase = BETTING;
}

void drawSuit(int suit, Vector2 center, float size, Color color)
{
    Texture2D art = suitArtwork[suit];
    if (art.id != 0) {
        if (suitPaletteLocation >= 0) {
            float palette = suit < 2 ? 0.0f : 1.0f;
            SetShaderValue(suitShader, suitPaletteLocation, &palette, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(suitShader);
        }
        DrawTexturePro(art, (Rectangle){0, 0, (float)art.width, (float)art.height},
                       (Rectangle){center.x - size / 2, center.y - size / 2, size, size},
                       (Vector2){0, 0}, 0, suitPaletteLocation >= 0 ? WHITE : color);
        if (suitPaletteLocation >= 0) { EndShaderMode(); }
        return;
    }
    float r = size * 0.26f;
    if (suit == 0) {
        DrawCircleV((Vector2){center.x - r, center.y - r * 0.45f}, r, color);
        DrawCircleV((Vector2){center.x + r, center.y - r * 0.45f}, r, color);
        DrawTriangle((Vector2){center.x - 2 * r, center.y - r * 0.1f}, (Vector2){center.x, center.y + 2.0f * r}, (Vector2){center.x + 2 * r, center.y - r * 0.1f}, color);
    }
    else if (suit == 1) { DrawPoly(center, 4, size * 0.62f, 0.0f, color); }
    else if (suit == 2) {
        DrawTriangle((Vector2){center.x, center.y - 2.0f * r}, (Vector2){center.x - 2 * r, center.y + r}, (Vector2){center.x + 2 * r, center.y + r}, color);
        DrawCircleV((Vector2){center.x - r * 0.7f, center.y + r * 0.5f}, r, color);
        DrawCircleV((Vector2){center.x + r * 0.7f, center.y + r * 0.5f}, r, color);
        DrawRectangle((int)(center.x - r * 0.35f), (int)center.y, (int)(r * 0.7f), (int)(r * 2.0f), color);
    }
    else {
        DrawCircleV((Vector2){center.x, center.y - r}, r, color);
        DrawCircleV((Vector2){center.x - r, center.y + r * 0.35f}, r, color);
        DrawCircleV((Vector2){center.x + r, center.y + r * 0.35f}, r, color);
        DrawRectangle((int)(center.x - r * 0.35f), (int)center.y, (int)(r * 0.7f), (int)(r * 2.0f), color);
    }
}

Texture2D loadCardTexture(const char *name)
{
    char relative[256];
    char executable[1024];
    char parent[1024];
    snprintf(relative, sizeof(relative), "games/blakjack/assetts/%s", name);
    snprintf(executable, sizeof(executable), "%s%s", GetApplicationDirectory(), relative);
    snprintf(parent, sizeof(parent), "%s../%s", GetApplicationDirectory(), relative);
    const char *path = NULL;
    if (FileExists(relative)) { path = relative; }
    else if (FileExists(executable)) { path = executable; }
    else if (FileExists(parent)) { path = parent; }
    if (path == NULL) { TraceLog(LOG_WARNING, "Blackjack artwork not found: %s", relative); return((Texture2D){0}); }
    Texture2D texture = LoadTexture(path);
    if (texture.id != 0) {
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
    }
    return(texture);
}

Shader loadBlackjackShader(const char *name)
{
    char relative[256];
    char executable[1024];
    char parent[1024];
    snprintf(relative, sizeof(relative), "shaders/%s", name);
    snprintf(executable, sizeof(executable), "%s%s", GetApplicationDirectory(), relative);
    snprintf(parent, sizeof(parent), "%s../%s", GetApplicationDirectory(), relative);
    if (FileExists(relative)) { return(LoadShader(NULL, relative)); }
    if (FileExists(executable)) { return(LoadShader(NULL, executable)); }
    if (FileExists(parent)) { return(LoadShader(NULL, parent)); }
    return((Shader){0});
}

void initCardLight(void)
{
    cardLightShader = loadBlackjackShader("blackjack_halo.fs");
    if (cardLightShader.id == 0) { return; }
    int quadLocation = GetShaderLocation(cardLightShader, "quadSize");
    int halfLocation = GetShaderLocation(cardLightShader, "cardHalfSize");
    int strength = GetShaderLocation(cardLightShader, "glowStrength");
    if (quadLocation < 0 || halfLocation < 0 || strength < 0) { UnloadShader(cardLightShader); cardLightShader = (Shader){0}; return; }
    Image white = GenImageColor(2, 2, WHITE);
    if (white.data == NULL) { UnloadShader(cardLightShader); cardLightShader = (Shader){0}; return; }
    cardLightTexture = LoadTextureFromImage(white);
    UnloadImage(white);
    if (cardLightTexture.id == 0) { UnloadShader(cardLightShader); cardLightShader = (Shader){0}; return; }
    Vector2 quadSize = {180.0f, 228.0f};
    Vector2 cardHalfSize = {60.0f, 84.0f};
    float strengthValue = 0.75f;
    SetShaderValue(cardLightShader, quadLocation, &quadSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(cardLightShader, halfLocation, &cardHalfSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(cardLightShader, strength, &strengthValue, SHADER_UNIFORM_FLOAT);
    cardLightReady = true;
}

void drawCardHalo(Vector2 center, Color color)
{
    DrawTexturePro(cardLightTexture, (Rectangle){0, 0, 2, 2},
                   (Rectangle){center.x - 90.0f, center.y - 114.0f, 180.0f, 228.0f},
                   (Vector2){0, 0}, 0, Fade(color, 0.65f));
}

void unloadCardLight(void)
{
    if (cardLightTexture.id != 0) { UnloadTexture(cardLightTexture); }
    if (cardLightReady) { UnloadShader(cardLightShader); }
}

void initCardArt(void)
{
    cardFaceArtwork = loadCardTexture("card-face.png");
    cardBackArtwork = loadCardTexture("card-back.png");
    static const char *const suitNames[] = {"suit-heart.png", "suit-diamond.png", "suit-spade.png", "suit-club.png"};
    static const char *const courtNames[2][3] = {
        {"court-pink-jack.png", "court-pink-queen.png", "court-pink-king-v2.png"},
        {"court-blue-jack.png", "court-blue-queen.png", "court-blue-king-v2.png"}
    };
    static const char *const legacyNames[] = {"court-jack.png", "court-queen.png", "court-king.png"};
    suitShader = loadBlackjackShader("blackjack_suit.fs");
    if (suitShader.id != 0) {
        suitPaletteLocation = GetShaderLocation(suitShader, "suitPalette");
        if (suitPaletteLocation < 0) { UnloadShader(suitShader); suitShader = (Shader){0}; }
    }
    for (int i = 0; i < 4; i++) {
        suitArtwork[i] = loadCardTexture(suitNames[i]);
    }
    for (int palette = 0; palette < 2; palette++) {
        for (int i = 0; i < 3; i++) {
            courtArtwork[palette][i] = loadCardTexture(courtNames[palette][i]);
            courtColored[palette][i] = courtArtwork[palette][i].id != 0;
            if (!courtColored[palette][i] && i != 2) { courtArtwork[palette][i] = loadCardTexture(legacyNames[i]); }
        }
    }
    buildingCardArt = true;
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 1; rank <= 13; rank++) {
            RenderTexture2D *frame = &cardFaces[suit][rank - 1];
            *frame = LoadRenderTexture(240, 336);
            if (frame->id == 0) { continue; }
            SetTextureFilter(frame->texture, TEXTURE_FILTER_BILINEAR);
            BeginTextureMode(*frame);
            ClearBackground(BLANK);
            Camera2D scale = {0};
            scale.zoom = 2.0f;
            BeginMode2D(scale);
            drawCard((Card){rank, suit}, 0.0f, 0.0f, false);
            EndMode2D();
            EndTextureMode();
        }
    }
    cardBack = LoadRenderTexture(240, 336);
    if (cardBack.id != 0) {
        SetTextureFilter(cardBack.texture, TEXTURE_FILTER_BILINEAR);
        BeginTextureMode(cardBack);
        ClearBackground(BLANK);
        Camera2D scale = {0};
        scale.zoom = 2.0f;
        BeginMode2D(scale);
        drawCard((Card){1, 0}, 0.0f, 0.0f, true);
        EndMode2D();
        EndTextureMode();
    }
    buildingCardArt = false;
}

void unloadCardArt(void)
{
    for (int suit = 0; suit < 4; suit++) {
        for (int rank = 0; rank < 13; rank++) {
            if (cardFaces[suit][rank].id != 0) { UnloadRenderTexture(cardFaces[suit][rank]); }
        }
    }
    if (cardBack.id != 0) { UnloadRenderTexture(cardBack); }
    if (cardFaceArtwork.id != 0) { UnloadTexture(cardFaceArtwork); }
    if (cardBackArtwork.id != 0) { UnloadTexture(cardBackArtwork); }
    for (int i = 0; i < 4; i++) { if (suitArtwork[i].id != 0) { UnloadTexture(suitArtwork[i]); } }
    for (int palette = 0; palette < 2; palette++) {
        for (int i = 0; i < 3; i++) { if (courtArtwork[palette][i].id != 0) { UnloadTexture(courtArtwork[palette][i]); } }
    }
    if (suitPaletteLocation >= 0) { UnloadShader(suitShader); }
}

void drawCard(Card card, float x, float y, bool hidden)
{
    Rectangle rect = {x, y, 120, 168};
    if (!buildingCardArt) {
        RenderTexture2D frame = hidden ? cardBack : cardFaces[card.suit][card.rank - 1];
        if (frame.id != 0) {
            DrawRectangleRounded((Rectangle){x + 5, y + 7, 120, 168}, 0.10f, 6, (Color){2, 5, 17, 170});
            DrawTexturePro(frame.texture, (Rectangle){0, 0, 240, -336}, rect, (Vector2){0, 0}, 0, WHITE);
            return;
        }
    }
    Texture2D art = hidden ? cardBackArtwork : cardFaceArtwork;
    if (art.id != 0) {
        DrawTexturePro(art, (Rectangle){0, 0, (float)art.width, (float)art.height}, rect, (Vector2){0, 0}, 0, WHITE);
    }
    else {
        DrawRectangleRounded(rect, 0.10f, 6, CARD);
        DrawRectangleRoundedLinesEx(rect, 0.10f, 6, 2.0f, hidden ? NEON_MAGENTA : CYAN);
        DrawRectangleLines((int)x + 7, (int)y + 7, 106, 154, Fade(NEON_PURPLE, 0.75f));
    }
    if (hidden) {
        if (art.id == 0) {
            DrawRectangleLines((int)x + 16, (int)y + 16, 88, 136, Fade(CYAN, 0.5f));
            DrawCircleLines((int)x + 60, (int)y + 84, 26, NEON_MAGENTA);
            DrawCircleLines((int)x + 60, (int)y + 84, 16, Fade(CYAN, 0.8f));
        }
        return;
    }
    static const char *const ranks[] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    Color color = card.suit < 2 ? NEON_MAGENTA : CYAN;
    cardDraw(ranks[card.rank], (int)x + 13, (int)y + 12, 25, TEXT);
    cardDraw(ranks[card.rank], (int)x + 91 - cardMeasure(ranks[card.rank], 18) / 2, (int)y + 136, 18, TEXT);
    if (card.rank == 1) { drawSuit(card.suit, (Vector2){x + 60, y + 86}, 52, color); }
    else if (card.rank >= 11) {
        int palette = card.suit < 2 ? 0 : 1;
        int index = card.rank - 11;
        Texture2D portrait = courtArtwork[palette][index];
        if (portrait.id != 0) {
            Rectangle source = {0, 0, (float)portrait.width, (float)portrait.height};
            float height = 92.0f;
            float width = height * portrait.width / portrait.height;
            if (width > 78.0f) { width = 78.0f; height = width * portrait.height / portrait.width; }
            Rectangle destination = {x + 60.0f - width / 2.0f, y + 88.0f - height / 2.0f, width, height};
            DrawTexturePro(portrait, source, destination, (Vector2){0, 0}, 0, courtColored[palette][index] ? WHITE : color);
        }
        else { cardDraw(ranks[card.rank], (int)x + 60 - cardMeasure(ranks[card.rank], 54) / 2, (int)y + 55, 54, TEXT); }
    }
    else {
        for (int i = 0; i < card.rank; i++) {
            Vector2 offset = {0};
            if (pipPosition(card.rank, i, &offset)) { drawSuit(card.suit, (Vector2){x + 60 + offset.x, y + 85 + offset.y}, 19, color); }
        }
    }
}

void drawHandArea(const Hand *hand, float y, bool hideSecond, float center, float maxSpan, const float *ages)
{
    if (hand->count <= 0) { return; }
    float gap = hand->count > 1 ? fminf(132.0f, maxSpan / (hand->count - 1)) : 0.0f;
    float width = 120.0f + gap * (hand->count - 1);
    float x = center - width / 2.0f;
    if (cardLightReady && cardLightEnabled) {
        BeginBlendMode(BLEND_ADDITIVE);
        BeginShaderMode(cardLightShader);
        for (int i = 0; i < hand->count; i++) {
            float progress = fminf(1.0f, ages[i] / 0.22f);
            float ease = 1.0f - (1.0f - progress) * (1.0f - progress);
            Color glow = hideSecond && i == 1 ? NEON_PURPLE : hand->cards[i].suit < 2 ? NEON_MAGENTA : CYAN;
            float centerX = x + gap * i + 60.0f;
            float centerY = y + (1.0f - ease) * 28.0f + 84.0f;
            drawCardHalo((Vector2){centerX, centerY}, glow);
        }
        EndShaderMode();
        EndBlendMode();
    }
    for (int i = 0; i < hand->count; i++) {
        float progress = fminf(1.0f, ages[i] / 0.22f);
        float ease = 1.0f - (1.0f - progress) * (1.0f - progress);
        drawCard(hand->cards[i], x + gap * i, y + (1.0f - ease) * 28.0f, hideSecond && i == 1);
    }
}

void drawCenter(const char *text, int y, int size, Color color)
{
    uiDraw(text, (WIDTH - uiMeasure(text, size)) / 2, y, size, color);
}

void drawGame(const Game *game)
{
    ClearBackground(BACKGROUND);
    for (int x = 0; x < WIDTH; x += 60) { DrawLine(x, 90, x, 840, (Color){15, 26, 46, 255}); }
    for (int y = 90; y < 840; y += 60) { DrawLine(0, y, WIDTH, y, (Color){15, 26, 46, 255}); }
    DrawRectangle(20, 18, WIDTH - 40, 55, TABLE);
    DrawRectangleLinesEx((Rectangle){20, 18, WIDTH - 40, 55}, 1.0f, Fade(NEON_PURPLE, 0.8f));
    uiDraw("NEUROS // BLACKJACK", 38, 32, 26, MINT);
    uiDraw(TextFormat("CHIPS %lld", (long long)game->chips), 660, 32, 26, TEXT);
    uiDraw(TextFormat("BET %lld", (long long)game->bet), 955, 32, 26, CYAN);
    DrawRectangleRounded((Rectangle){135, 98, 930, 670}, 0.05f, 6, Fade(TABLE, 0.88f));
    DrawRectangleRoundedLinesEx((Rectangle){135, 98, 930, 670}, 0.05f, 6, 2.0f, Fade(NEON_PURPLE, 0.7f));
    uiDraw("DEALER", 174, 116, 26, NEON_MAGENTA);
    if (!game->split) { uiDraw("PLAYER", 174, 448, 26, MINT); }
    else {
        Color left = game->phase == PLAYER_TURN && game->activeHand == 0 ? MINT : MUTED;
        Color right = game->phase == PLAYER_TURN && game->activeHand == 1 ? MINT : MUTED;
        uiDraw(TextFormat("HAND 1  %d  /  BET %lld", handValue(&game->player), (long long)game->bet), 195, 448, 22, left);
        uiDraw(TextFormat("HAND 2  %d  /  BET %lld", handValue(&game->splitHand), (long long)game->splitBet), 655, 448, 22, right);
    }
    if (game->insuranceBet > 0 && game->phase != ROUND_OVER) { uiDraw("INSURED", 174, 150, 18, CYAN); }
    bool hideDealer = game->phase == PLAYER_TURN || game->phase == INSURANCE;
    if (game->dealer.count > 0) {
        if (hideDealer) {
            Hand visible = {0};
            visible.cards[visible.count++] = game->dealer.cards[0];
            uiDraw(TextFormat("%d + ?", handValue(&visible)), 950, 116, 26, TEXT);
        }
        else { uiDraw(TextFormat("%d", handValue(&game->dealer)), 950, 116, 26, TEXT); }
    }
    if (!game->split && game->player.count > 0) { uiDraw(TextFormat("%d", handValue(&game->player)), 950, 448, 26, TEXT); }
    drawHandArea(&game->dealer, 155, hideDealer, WIDTH / 2.0f, 760.0f, dealerCardAge);
    if (game->split) {
        drawHandArea(&game->player, 488, false, 390.0f, 280.0f, playerCardAge);
        drawHandArea(&game->splitHand, 488, false, 810.0f, 280.0f, splitCardAge);
    }
    else { drawHandArea(&game->player, 488, false, WIDTH / 2.0f, 760.0f, playerCardAge); }
    DrawLine(175, 393, 1025, 393, Fade(NEON_PURPLE, 0.45f));
    if (game->phase == BETTING) {
        drawCenter("PLACE YOUR BET", 220, 28, TEXT);
        drawCenter("ARROWS: BET   SHIFT + ARROWS: FAST BET   ENTER: DEAL", 270, 20, MUTED);
        drawTableButton((Rectangle){380, 305, 120, 40}, "- BET", CYAN, true, false);
        drawTableButton((Rectangle){510, 305, 180, 40}, "ENTER  DEAL", MINT, true, false);
        drawTableButton((Rectangle){700, 305, 120, 40}, "+ BET", CYAN, true, false);
        if (game->randomError) { drawCenter("RANDOM SOURCE UNAVAILABLE // RETRY", 352, 16, NEON_MAGENTA); }
    }
    else if (game->phase == INSURANCE) {
        drawCenter("DEALER SHOWS ACE // INSURANCE?", 353, 25, CYAN);
        drawTableButton((Rectangle){400, 390, 190, 40}, TextFormat("I  INSURE %lld", (long long)(game->bet / 2)), MINT, true, false);
        drawTableButton((Rectangle){610, 390, 190, 40}, "N  DECLINE", CYAN, true, false);
    }
    else if (game->phase == PLAYER_TURN) {
        const Hand *hand = currentPlayerHand(game);
        int64_t wager = game->activeHand == 0 ? game->bet : game->splitBet;
        bool canDouble = !game->splitAces && hand->count == 2 && game->chips >= wager;
        bool canSplit = !game->split && hand->count == 2 && hand->cards[0].rank == hand->cards[1].rank && game->chips >= game->bet;
        bool canSurrender = !game->split && hand->count == 2;
        const char *const labels[] = {"H  HIT", "S  STAND", "D  DOUBLE", "P  SPLIT", "R  SURRENDER"};
        bool available[] = {true, true, canDouble, canSplit, canSurrender};
        int keys[] = {KEY_H, KEY_S, KEY_D, KEY_P, KEY_R};
        for (int i = 0; i < 5; i++) {
            Rectangle action = blackjackActionRect(i);
            Vector2 pointer = blackjackPointer(GetMousePosition(), GetScreenWidth(), GetScreenHeight());
            bool pressed = available[i] && (IsKeyDown(keys[i]) || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(pointer, action)));
            drawTableButton(action, labels[i], i == 1 ? CYAN : MINT, available[i], pressed);
        }
    }
    else if (game->phase == DEALER_TURN) { drawCenter("DEALER PLAYS", 371, 22, CYAN); }
    else {
        static const char *const messages[] = {"", "BLACKJACK // 3:2", "YOU WIN", "DEALER WINS", "PUSH", "BUST", "SURRENDER"};
        Color resultColor = game->result == PLAYER_WINS || game->result == PLAYER_BLACKJACK ? MINT : game->result == PUSH ? CYAN : NEON_MAGENTA;
        if (game->split) {
            uiDraw(messages[game->result], 280, 680, 22, resultColor);
            Color secondColor = game->splitResult == PLAYER_WINS ? MINT : game->splitResult == PUSH ? CYAN : NEON_MAGENTA;
            uiDraw(messages[game->splitResult], 700, 680, 22, secondColor);
            drawCenter("ROUND COMPLETE", 355, 25, TEXT);
        }
        else { drawCenter(messages[game->result], 355, 27, resultColor); }
        int64_t net = game->chips - game->roundStartChips;
        drawCenter(TextFormat("NET %+lld", (long long)net), 390, 22, net > 0 ? MINT : net < 0 ? NEON_MAGENTA : TEXT);
        if (game->insuranceBet > 0) {
            uiDraw(isBlackjack(&game->dealer) ? "INSURANCE WON" : "INSURANCE LOST", 175, 715, 18, MUTED);
        }
        if (game->chips >= MIN_BET) { drawTableButton((Rectangle){470, 421, 260, 28}, "ENTER: NEXT HAND", CYAN, true, false); }
        else { drawCenter("OUT OF CHIPS // R: RESTART", 425, 19, TEXT); }
        if (game->randomError) { drawCenter("RANDOM SOURCE UNAVAILABLE // R TO RETRY", 452, 15, NEON_MAGENTA); }
    }
    drawCenter("BLACKJACK PAYS 3:2   //   DEALER STANDS ON SOFT 17", 790, 21, MUTED);
    drawCenter("MOUSE / KEYS    F1: RULES    F: FX    M: SOUND    F11: FULLSCREEN    ESC: QUIT", 836, 20, MUTED);
}
