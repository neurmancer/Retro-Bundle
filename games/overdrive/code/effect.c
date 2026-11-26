#include <stdbool.h>
#include <stdio.h>
#include <raylib.h>

typedef struct {
    RenderTexture2D target;
    Shader shader;
    int timeLocation;
    int width;
    int height;
    int targetScale;
    int windowWidth;
    int windowHeight;
    Vector2 windowPosition;
    bool ready;
    bool enabled;
} NeurosEffect;

NeurosEffect loadNeurosEffectNamed(int width, int height, float strength, Vector3 accent, const char *filename)
{
    NeurosEffect effect = {0};
    effect.width = width;
    effect.height = height;
    effect.targetScale = 1;
    effect.target = LoadRenderTexture(width, height);
    if (effect.target.id == 0) { TraceLog(LOG_WARNING, "NeurOS render target could not be created"); }
    char executablePath[1024];
    char parentPath[1024];
    snprintf(executablePath, sizeof(executablePath), "%sshaders/%s", GetApplicationDirectory(), filename);
    snprintf(parentPath, sizeof(parentPath), "%s../shaders/%s", GetApplicationDirectory(), filename);
    const char *path = NULL;
    char localPath[1024];
    char bundlePath[1024];
    char bundleParentPath[1024];
    snprintf(localPath, sizeof(localPath), "shaders/%s", filename);
    snprintf(bundlePath, sizeof(bundlePath), "games/overdrive/shaders/%s", filename);
    snprintf(bundleParentPath, sizeof(bundleParentPath), "%s../games/overdrive/shaders/%s", GetApplicationDirectory(), filename);
    if (FileExists(bundlePath)) { path = bundlePath; }
    else if (FileExists(bundleParentPath)) { path = bundleParentPath; }
    else if (FileExists(localPath)) { path = localPath; }
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
    Vector2 resolution = {(float)width, (float)height};
    SetShaderValue(effect.shader, resolutionLocation, &resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(effect.shader, strengthLocation, &strength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(effect.shader, accentLocation, &accent, SHADER_UNIFORM_VEC3);
    effect.ready = true;
    effect.enabled = true;
    return(effect);
}

NeurosEffect loadNeurosEffect(int width, int height, float strength, Vector3 accent)
{
    return(loadNeurosEffectNamed(width, height, strength, accent, "neuros_retro.fs"));
}

bool setNeurosRenderScale(NeurosEffect *effect, int scale)
{
    if (scale < 1 || scale > 2 || effect->width <= 0 || effect->height <= 0) { return(false); }
    if (effect->target.id != 0 && scale == effect->targetScale) { return(true); }
    RenderTexture2D target = LoadRenderTexture(effect->width * scale, effect->height * scale);
    if (target.id == 0) { return(false); }
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    if (effect->target.id != 0) { UnloadRenderTexture(effect->target); }
    effect->target = target;
    effect->targetScale = scale;
    if (effect->ready) {
        int location = GetShaderLocation(effect->shader, "resolution");
        Vector2 resolution = {(float)effect->target.texture.width, (float)effect->target.texture.height};
        SetShaderValue(effect->shader, location, &resolution, SHADER_UNIFORM_VEC2);
    }
    return(true);
}

Rectangle neurosViewport(int width, int height, int screenWidth, int screenHeight)
{
    if (width <= 0 || height <= 0 || screenWidth <= 0 || screenHeight <= 0) { return((Rectangle){0}); }
    float scaleX = (float)screenWidth / width;
    float scaleY = (float)screenHeight / height;
    float scale = scaleX < scaleY ? scaleX : scaleY;
    float outputWidth = width * scale;
    float outputHeight = height * scale;
    return((Rectangle){(screenWidth - outputWidth) / 2.0f, (screenHeight - outputHeight) / 2.0f, outputWidth, outputHeight});
}

void toggleNeurosFullscreen(NeurosEffect *effect)
{
    if (IsWindowFullscreen()) {
        ToggleFullscreen();
        if (effect->windowWidth > 0 && effect->windowHeight > 0) {
            SetWindowSize(effect->windowWidth, effect->windowHeight);
            SetWindowPosition((int)effect->windowPosition.x, (int)effect->windowPosition.y);
        }
    }
    else {
        effect->windowWidth = GetScreenWidth();
        effect->windowHeight = GetScreenHeight();
        effect->windowPosition = GetWindowPosition();
        ToggleFullscreen();
    }
}

void beginNeurosEffect(NeurosEffect *effect)
{
    if (effect->target.id != 0) {
        BeginTextureMode(effect->target);
        if (effect->targetScale > 1) {
            Camera2D camera = {0};
            camera.zoom = (float)effect->targetScale;
            BeginMode2D(camera);
        }
    }
    else {
        BeginDrawing();
        Rectangle viewport = neurosViewport(effect->width, effect->height, GetScreenWidth(), GetScreenHeight());
        Camera2D camera = {0};
        camera.offset = (Vector2){viewport.x, viewport.y};
        camera.zoom = viewport.width / effect->width;
        BeginMode2D(camera);
    }
}

void endNeurosEffect(NeurosEffect *effect)
{
    if (effect->target.id == 0) { EndMode2D(); EndDrawing(); return; }
    if (effect->targetScale > 1) { EndMode2D(); }
    EndTextureMode();
    BeginDrawing();
    ClearBackground(BLACK);
    if (effect->ready && effect->enabled) {
        float time = (float)GetTime();
        SetShaderValue(effect->shader, effect->timeLocation, &time, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(effect->shader);
    }
    Rectangle source = {0, 0, (float)effect->target.texture.width, -(float)effect->target.texture.height};
    Rectangle viewport = neurosViewport(effect->width, effect->height, GetScreenWidth(), GetScreenHeight());
    DrawTexturePro(effect->target.texture, source, viewport, (Vector2){0, 0}, 0, WHITE);
    if (effect->ready && effect->enabled) { EndShaderMode(); }
    EndDrawing();
}

void unloadNeurosEffect(NeurosEffect *effect)
{
    if (effect->target.id != 0) { UnloadRenderTexture(effect->target); }
    if (effect->ready) { UnloadShader(effect->shader); }
}
