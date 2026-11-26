#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform float time;
uniform float strength;
uniform vec3 accent;
void main()
{
    vec2 uv = fragTexCoord;
    vec2 pixel = 1.0 / resolution;
    vec3 base = texture(texture0, uv).rgb;
    vec3 glow = (texture(texture0, uv + vec2(pixel.x * 2.0, 0.0)).rgb +
                 texture(texture0, uv - vec2(pixel.x * 2.0, 0.0)).rgb +
                 texture(texture0, uv + vec2(0.0, pixel.y * 2.0)).rgb +
                 texture(texture0, uv - vec2(0.0, pixel.y * 2.0)).rgb) * 0.25;
    glow = max(glow - vec3(0.58), vec3(0.0));
    vec2 sunDelta = (uv - vec2(0.5, 0.65)) * vec2(resolution.x / resolution.y, 1.0);
    float halo = exp(-dot(sunDelta, sunDelta) * 16.0) * smoothstep(0.48, 0.58, uv.y);
    float flicker = 0.98 + 0.02 * sin(time * 0.5);
    vec3 treated = base + glow * (vec3(0.08) + accent * 0.08) + vec3(1.0, 0.22, 0.31) * halo * 0.09 * flicker;
    finalColor = vec4(mix(base, clamp(treated, 0.0, 1.0), clamp(strength, 0.0, 1.0)), 1.0) * fragColor;
}
