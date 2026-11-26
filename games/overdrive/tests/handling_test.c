#include <assert.h>
#include <math.h>
#define main overdrive_application_main
#include "../overdrive.c"
#undef main

int main(void)
{
    Drive game;
    resetDrive(&game);
    for (int i = 0; i < TRAFFIC_COUNT; i++) { assert(game.traffic[i].color >= 0 && game.traffic[i].color < TRAFFIC_MODEL_COUNT); }
    for (int i = 0; i < 100; i++) { updateHandling(&game, 0.01f, true, false, 0); }
    assert(game.speed > 10.0f && game.speed < 12.0f);
    assert(game.x == 0.0f);
    float moving = game.speed;
    updateHandling(&game, 0.1f, false, false, 1);
    assert(game.speed < moving && game.speed > moving - 1.0f);
    assert(game.lateralSpeed > 0.0f && game.x > 0.0f);
    float lateral = game.lateralSpeed;
    updateHandling(&game, 0.01f, false, false, 0);
    assert(game.lateralSpeed > 0.0f && game.lateralSpeed < lateral);
    for (int i = 0; i < 100; i++) { updateHandling(&game, 0.01f, false, true, 0); }
    assert(game.speed == 0.0f && game.lateralSpeed == 0.0f);
    game.x = ROAD_HALF;
    game.speed = 30.0f;
    updateHandling(&game, 0.1f, false, false, 1);
    assert(game.speed < 29.4f && game.lateralSpeed < 0.4f);
    for (int i = 0; i < 100; i++) { updateHandling(&game, 0.01f, true, false, 1); }
    assert(game.x <= ROAD_HALF + 1.0f && game.lateralSpeed == 0.0f);
    updateHandling(&game, 0.1f, false, false, -1);
    assert(game.lateralSpeed < 0.0f && game.x < ROAD_HALF + 1.0f);
    resetDrive(&game);
    for (int i = 0; i < 10000; i++) { updateHandling(&game, 0.01f, true, false, 0); }
    assert(fabsf(game.speed * 3.6f - 225.0f) < 0.01f);
    resetDrive(&game);
    game.speed = 45.0f;
    Traffic ahead = {.x = 3.0f, .z = -40.0f, .speed = 15.0f};
    Traffic behind = {.x = 3.0f, .z = 40.0f, .speed = 15.0f};
    assert(trafficDopplerPitch(&game, &ahead) > 1.0f);
    assert(trafficDopplerPitch(&game, &behind) < 1.0f);
    ahead.speed = game.speed;
    assert(fabsf(trafficDopplerPitch(&game, &ahead) - 1.0f) < 0.0001f);
    ahead.x = game.x;
    ahead.z = 0;
    assert(trafficDopplerPitch(&game, &ahead) == 1.0f);
    return(0);
}
