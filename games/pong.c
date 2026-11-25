#include <math.h>
#include <stdio.h>
#include <raylib.h>
#include <stdlib.h>  // for rand
#include <time.h>


#define WIDTH 1200
#define HEIGHT 900

#define FPS 120

#define PADDLE_WIDTH (WIDTH / 120.0f)
#define PADDLE_HEIGHT (HEIGHT / 8.0f)
#define PADDLE_SPEED 450.0f

#define BALL_RADIUS 6.0f
#define MAX_SCORE 7


/*  ========== COLORS ===========  */

/*  Backgrounds */

#define PURPLISH_BG (Color){50,20,45,127}
#define VIOLET_BG (Color){12,10,62,255}

/* Paddles & ball*/

#define NEON_LINE (Color){158,255,31,255}
#define CRIMSON (Color){153,0,51,255}







/* =========== OBJECTS ============ */
typedef struct {
    Vector2 position;
    Vector2 size;

    float speed;
    
    int score;
    
    Color color;

} Player;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    
    float radius;
    
    Color color;

} Ball;

typedef struct {
    Color bgColor;
    Color objectColor;

} Theme;

/* =============  FUNCTION PROTOTYPES  ============== */
void UpdateBall(Ball *b, float dt);
void CheckBallCollisionWithPaddles(Ball *b, Player *p1, Player *p2);
int CheckScore(Ball *b);
void ResetBall(Ball *b);

int main(void) {

    srand(time(NULL));

    InitWindow(WIDTH, HEIGHT, "PONG DUH... ");
    if (!IsWindowReady()) { printf("Window may not be ready but Linux is B;)\n"); return(-53); }
    SetTargetFPS(FPS);

    Theme themes[] = {
        {BLACK, WHITE},
        {WHITE, BLACK},
        {PURPLISH_BG, NEON_LINE},
        {VIOLET_BG, CRIMSON}
    };
    int currentTheme = 0;
    Theme selected = themes[currentTheme];

    Player p1 = {0};
    p1.position = (Vector2){40, HEIGHT/2.0f - PADDLE_HEIGHT/2.0f};
    p1.size = (Vector2){PADDLE_WIDTH, PADDLE_HEIGHT};
    p1.speed = PADDLE_SPEED;
    p1.score = 0;
    p1.color = selected.objectColor;

    Player p2 = {0};
    p2.position = (Vector2){WIDTH - 40 - PADDLE_WIDTH, HEIGHT/2.0f - PADDLE_HEIGHT/2.0f};
    p2.size = (Vector2){PADDLE_WIDTH, PADDLE_HEIGHT};
    p2.speed = PADDLE_SPEED;
    p2.score = 0;
    p2.color = selected.objectColor;

    Ball ball = {0};
    ball.position = (Vector2){WIDTH/2.0f, HEIGHT/2.0f};
    ball.radius = BALL_RADIUS;
    ball.velocity = (Vector2){320.0f, 180.0f};
    ball.color = selected.objectColor;

    bool gameOver = false;
    bool dashed = false;

    int winner = 0;


    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_TAB)) {
            currentTheme = (currentTheme + 1) % 4;
            selected = themes[currentTheme];
            p1.color = selected.objectColor;
            p2.color = selected.objectColor;
            ball.color = selected.objectColor;
        }

        if (IsKeyPressed(KEY_ESCAPE)) { CloseWindow(); }
        if (IsKeyPressed(KEY_E)) {
            if(dashed) {dashed = false; } 
            else { dashed = true; }
    
        }
        
        if (!gameOver) {
            
            // === CONTROLS ===
            

            if (IsKeyDown(KEY_W)) p1.position.y -= p1.speed * dt;
            if (IsKeyDown(KEY_S)) p1.position.y += p1.speed * dt;
            
            if (IsKeyDown(KEY_UP)) p2.position.y -= p2.speed * dt;
            if (IsKeyDown(KEY_DOWN)) p2.position.y += p2.speed * dt;

            // Clamp paddles
            if (p1.position.y < 0) p1.position.y = 0;
            if (p1.position.y + p1.size.y > HEIGHT) p1.position.y = HEIGHT - p1.size.y;
            if (p2.position.y < 0) p2.position.y = 0;
            if (p2.position.y + p2.size.y > HEIGHT) p2.position.y = HEIGHT - p2.size.y;

            UpdateBall(&ball, dt);
            CheckBallCollisionWithPaddles(&ball, &p1, &p2);

            int scoreChange = CheckScore(&ball);
            if (scoreChange != 0) {
                if (scoreChange == 1) { p2.score++; }
                else { p1.score++; }

                ResetBall(&ball);

                if (p1.score >= MAX_SCORE || p2.score >= MAX_SCORE) {
                    gameOver = true;
                    winner = (p1.score >= MAX_SCORE) ? 1 : 2;
                }
            }
        } else if (IsKeyPressed(KEY_R)) {
            p1.score = 0;
            p2.score = 0;
            gameOver = false;
            ResetBall(&ball);
        }

        // === DRAWING ===
        BeginDrawing();
        ClearBackground(selected.bgColor);

        if (dashed) {
            for (int i = 0; i < HEIGHT; i += 40) {
                DrawRectangle(WIDTH/2 - 4, i, 8, 20, selected.objectColor);
            }
        }
        
        DrawRectangleV(p1.position, p1.size, p1.color);
        DrawRectangleV(p2.position, p2.size, p2.color);
        DrawCircleV(ball.position, ball.radius, ball.color);

        DrawText(TextFormat("%d", p2.score), WIDTH/4, 40, 80, selected.objectColor);
        DrawText(TextFormat("%d", p1.score), 3*WIDTH/4 - 60, 40, 80, selected.objectColor);

        if (gameOver) {
            const char* msg = (winner == 1) ? "PLAYER 2 WINS, BABY!!!" : "PLAYER 1 WINS BABY!!!";
            DrawText(msg, WIDTH/2 - MeasureText(msg, 60)/2, HEIGHT/2 - 80, 60, RED);
            DrawText("PRESS R TO REMATCH", WIDTH/2 - MeasureText("PRESS R TO REMATCH", 40)/2, HEIGHT/2 + 20, 40, selected.objectColor);
        }

        DrawText("TAB = Theme | W/S = P1 | UP/DOWN = P2 | ESC = Quit", 20, 20, 12, selected.objectColor);

        EndDrawing();
    }

    CloseWindow();
    return(0);
}

void UpdateBall(Ball *b, float dt) {
    b->position.x += b->velocity.x * dt;
    b->position.y += b->velocity.y * dt;

    // Top/Bottom bounce
    if (b->position.y - b->radius <= 0) {
        b->position.y = b->radius;
        b->velocity.y = fabsf(b->velocity.y);
    }
    if (b->position.y + b->radius >= HEIGHT) {
        b->position.y = HEIGHT - b->radius;
        b->velocity.y = -fabsf(b->velocity.y);
    }
}

void CheckBallCollisionWithPaddles(Ball *b, Player *p1, Player *p2) {
    Rectangle p1Rec = {p1->position.x, p1->position.y, p1->size.x, p1->size.y};
    if (CheckCollisionCircleRec(b->position, b->radius, p1Rec)) {
        b->velocity.x = fabsf(b->velocity.x) * 1.05f;  // slight speed up
        float hit = (b->position.y - (p1->position.y + p1->size.y/2)) / (p1->size.y/2);
        b->velocity.y = hit * 420.0f;
    }

    Rectangle p2Rec = {p2->position.x, p2->position.y, p2->size.x, p2->size.y};
    if (CheckCollisionCircleRec(b->position, b->radius, p2Rec)) {
        b->velocity.x = -fabsf(b->velocity.x) * 1.05f;
        float hit = (b->position.y - (p2->position.y + p2->size.y/2)) / (p2->size.y/2);
        b->velocity.y = hit * 420.0f;
    }
}

void ResetBall(Ball *b) {
    b->position = (Vector2){WIDTH/2.0f, HEIGHT/2.0f};
    int dir = (rand() % 2) ? 1 : -1;
    b->velocity.x = dir * (300.0f + (rand() % 100));
    b->velocity.y = (rand() % 360) - 180.0f;
}

int CheckScore(Ball *b) {
    if (b->position.x < 0) return(-1);   // P1 gets score
    if (b->position.x > WIDTH) return(1); // P2 gets score
    return(0);
}



