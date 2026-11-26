#include <stdbool.h>
#include <stdio.h>
#include <raylib.h>
#include <rlgl.h>

typedef enum {
    CAR_TRUCK,
    CAR_MERCEDES,
    CAR_CLIO,
    TRAFFIC_MODEL_COUNT,
    CAR_SENNA = TRAFFIC_MODEL_COUNT,
    CAR_MODEL_COUNT
} CarModel;

typedef struct {
    Model models[CAR_MODEL_COUNT];
    bool loaded[CAR_MODEL_COUNT];
    Model streetlight;
    bool streetlightLoaded;
    Shader lighting;
    bool lightingReady;
} DriveModels;

const char *driveAssetPath(const char *relative, char *result, size_t size)
{
    const char *formats[] = {"%s", "%s%s", "%s../%s", "games/overdrive/%s", "%s../games/overdrive/%s"};
    for (int i = 0; i < 5; i++) {
        if (i == 1 || i == 2 || i == 4) { snprintf(result, size, formats[i], GetApplicationDirectory(), relative); }
        else { snprintf(result, size, formats[i], relative); }
        if (FileExists(result)) { return(result); }
    }
    return(NULL);
}

void setDriveLighting(Model *model, Shader shader)
{
    for (int i = 0; i < model->materialCount; i++) { model->materials[i].shader = shader; }
}

bool loadDriveModels(DriveModels *cars)
{
    static const char *const names[CAR_MODEL_COUNT] = {"truck_imported", "mercedes_imported", "clio", "senna_imported"};
    *cars = (DriveModels){0};
    char vertexPath[1024], fragmentPath[1024];
    if (driveAssetPath("shaders/lighting.vs", vertexPath, sizeof(vertexPath)) &&
        driveAssetPath("shaders/lighting.fs", fragmentPath, sizeof(fragmentPath))) {
        cars->lighting = LoadShader(vertexPath, fragmentPath);
        cars->lightingReady = cars->lighting.id != 0 && GetShaderLocation(cars->lighting, "matNormal") >= 0;
    }
    for (int i = 0; i < CAR_MODEL_COUNT; i++) {
        char relative[256], resolved[1024];
        snprintf(relative, sizeof(relative), "assets/models/%s.glb", names[i]);
        const char *path = driveAssetPath(relative, resolved, sizeof(resolved));
        if (path == NULL) { TraceLog(LOG_ERROR, "Overdrive model not found: %s", relative); return(false); }
        cars->models[i] = LoadModel(path);
        cars->loaded[i] = cars->models[i].meshCount > 0 && cars->models[i].meshes != NULL;
        if (!cars->loaded[i]) { TraceLog(LOG_ERROR, "Could not load Overdrive model: %s", path); return(false); }
        if (cars->lightingReady) { setDriveLighting(&cars->models[i], cars->lighting); }
    }
    char lightPath[1024];
    if (driveAssetPath("assets/models/streetlight.glb", lightPath, sizeof(lightPath))) {
        cars->streetlight = LoadModel(lightPath);
        cars->streetlightLoaded = cars->streetlight.meshCount > 0 && cars->streetlight.meshes != NULL;
        if (cars->streetlightLoaded && cars->lightingReady) { setDriveLighting(&cars->streetlight, cars->lighting); }
    }
    return(true);
}

void unloadDriveModels(DriveModels *cars)
{
    for (int i = 0; i < CAR_MODEL_COUNT; i++) {
        if (cars->loaded[i]) { UnloadModel(cars->models[i]); cars->loaded[i] = false; }
    }
    if (cars->streetlightLoaded) { UnloadModel(cars->streetlight); cars->streetlightLoaded = false; }
    if (cars->lightingReady) { UnloadShader(cars->lighting); cars->lightingReady = false; }
}

void drawCarModel(const DriveModels *cars, CarModel model, float x, float z)
{
    if (model < 0 || model >= CAR_MODEL_COUNT || !cars->loaded[model]) { return; }
    rlDisableBackfaceCulling();
    DrawModel(cars->models[model], (Vector3){x, 0, z}, 1.0f, WHITE);
    rlEnableBackfaceCulling();
}
