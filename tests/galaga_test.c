#include <assert.h>
#include <math.h>
#define main galaga_application_main
#include "../games/galaga.c"
#undef main

int main(void)
{
    Game game = {0};
    srand(1);
    resetGame(&game);
    assert(game.lives == 3 && game.remaining == ALIEN_COUNT && game.wave == 1);
    float initialY = game.player.y;
    updatePlayer(&game, 0.1f, 0, -1);
    assert(game.player.y < initialY && game.playerVelocity.y < 0.0f);
    for (int i = 0; i < 100; i++) { updatePlayer(&game, 0.05f, -1, -1); }
    assert(game.player.x >= 35.0f && game.player.y >= 355.0f);

    resetGame(&game);
    game.shots[0] = (Shot){.position = {100, 600}, .velocity = {0, -560}, .active = true, .friendly = true, .acceleration = 800};
    updateShots(&game, 0.05f);
    assert(game.shots[0].speed > 560.0f);
    game.shots[1] = (Shot){.position = {1100, 500}, .velocity = {0, 250}, .active = true, .friendly = false, .acceleration = -130};
    updateShots(&game, 0.05f);
    float slowed = game.shots[1].speed;
    assert(slowed < 250.0f);
    game.shots[1].age = 0.7f;
    updateShots(&game, 0.05f);
    assert(game.shots[1].speed > slowed);

    game.shots[2] = (Shot){.position = game.aliens[0].position, .velocity = {0, 0}, .active = true, .friendly = true};
    updateShots(&game, 0.0f);
    assert(game.aliens[0].alive && game.aliens[0].health == 1 && game.remaining == ALIEN_COUNT && game.score == 0);
    game.shots[2] = (Shot){.position = game.aliens[0].position, .velocity = {0, 0}, .active = true, .friendly = true};
    updateShots(&game, 0.0f);
    assert(!game.aliens[0].alive && game.remaining == ALIEN_COUNT - 1 && game.score == 100);
    game.shots[2] = (Shot){.position = game.aliens[COLUMNS].position, .velocity = {0, 0}, .active = true, .friendly = true};
    updateShots(&game, 0.0f);
    assert(!game.aliens[COLUMNS].alive && game.remaining == ALIEN_COUNT - 2 && game.score == 200);
    game.invulnerability = 0.0f;
    game.shots[3] = (Shot){.position = game.player, .velocity = {0, 0}, .active = true};
    updateShots(&game, 0.0f);
    assert(game.lives == 2 && game.invulnerability > 0.0f);
    assert(soundPitch((Vector2){0, 0}, (Vector2){0, 250}, (Vector2){0, 500}, (Vector2){0}) > 1.0f);
    assert(soundPitch((Vector2){0, 0}, (Vector2){0, -250}, (Vector2){0, 500}, (Vector2){0}) < 1.0f);
    Vector2 listener = {500, 500};
    Vector2 velocity = {0, 500};
    float approaching = soundPitch((Vector2){490, 450}, velocity, listener, (Vector2){0});
    float passing = soundPitch((Vector2){490, 500}, velocity, listener, (Vector2){0});
    float receding = soundPitch((Vector2){490, 550}, velocity, listener, (Vector2){0});
    assert(approaching > passing && passing > receding);
    assert(soundPitch((Vector2){490, 450}, velocity, listener, (Vector2){0, -100}) > approaching);
    assert(soundPitch((Vector2){500, 500}, velocity, listener, (Vector2){0}) == 1.0f);
    Color blue = shiftColor((Vector2){600, 400}, (Vector2){0, 250}, NEON_VIOLET);
    Color red = shiftColor((Vector2){600, 400}, (Vector2){0, -250}, NEON_VIOLET);
    assert(blue.b > NEON_VIOLET.b && red.r > NEON_VIOLET.r);
    game.wave = 2;
    startWave(&game);
    assert(game.remaining == ALIEN_COUNT && game.wave == 2);
    for (int i = 0; i < SHOT_COUNT; i++) { assert(!game.shots[i].active); }
    return(0);
}
