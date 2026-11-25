#include <stdbool.h>
#include <stdio.h>
#include <raylib.h>

typedef struct {
    RenderTexture2D target;
    Shader shader;
    int timeLocation;
    bool ready;
    bool enabled;
} NeurosEffect;

NeurosEffect loadNeurosEffect(int width, int height, float strength, Vector3 accent)
{
    NeurosEffect effect = {0};
    char executablePath[1024];
    char parentPath[1024];
    snprintf(executablePath, sizeof(executablePath), "%sshaders/neuros_retro.fs", GetApplicationDirectory());
    snprintf(parentPath, sizeof(parentPath), "%s../shaders/neuros_retro.fs", GetApplicationDirectory());
    const char *path = NULL;
    if (FileExists("shaders/neuros_retro.fs")) { path = "shaders/neuros_retro.fs"; }
    else if (FileExists(executablePath)) { path = executablePath; }
    else if (FileExists(parentPath)) { path = parentPath; }
    if (path == NULL) { TraceLog(LOG_WARNING, "NeurOS shader not found; drawing without effects"); return(effect); }

    effect.shader = LoadShader(NULL, path);
    int resolutionLocation = GetShaderLocation(effect.shader, "resolution");
    int strengthLocation = GetShaderLocation(effect.shader, "strength");
    int accentLocation = GetShaderLocation(effect.shader, "accent");
    effect.timeLocation = GetShaderLocation(effect.shader, "time");
    if (resolutionLocation < 0 || strengthLocation < 0 || accentLocation < 0 || effect.timeLocation < 0) {
        TraceLog(LOG_WARNING, "NeurOS shader could not be loaded; drawing without effects");
        UnloadShader(effect.shader);
        effect.shader = (Shader){0};
        return(effect);
    }
    effect.target = LoadRenderTexture(width, height);
    if (effect.target.id == 0) {
        TraceLog(LOG_WARNING, "NeurOS render target could not be created; drawing without effects");
        UnloadShader(effect.shader);
        effect.shader = (Shader){0};
        return(effect);
    }
    Vector2 resolution = {(float)width, (float)height};
    SetShaderValue(effect.shader, resolutionLocation, &resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(effect.shader, strengthLocation, &strength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(effect.shader, accentLocation, &accent, SHADER_UNIFORM_VEC3);
    effect.ready = true;
    effect.enabled = true;
    return(effect);
}

void beginNeurosEffect(NeurosEffect *effect)
{
    if (effect->ready && effect->enabled) { BeginTextureMode(effect->target); }
    else { BeginDrawing(); }
}

void endNeurosEffect(NeurosEffect *effect)
{
    if (!effect->ready || !effect->enabled) { EndDrawing(); return; }
    EndTextureMode();
    float time = (float)GetTime();
    SetShaderValue(effect->shader, effect->timeLocation, &time, SHADER_UNIFORM_FLOAT);
    BeginDrawing();
    ClearBackground(BLACK);
    BeginShaderMode(effect->shader);
    DrawTextureRec(effect->target.texture, (Rectangle){0, 0, (float)effect->target.texture.width, -(float)effect->target.texture.height}, (Vector2){0, 0}, WHITE);
    EndShaderMode();
    EndDrawing();
}

void unloadNeurosEffect(NeurosEffect *effect)
{
    if (effect->ready) {
        UnloadRenderTexture(effect->target);
        UnloadShader(effect->shader);
    }
}
