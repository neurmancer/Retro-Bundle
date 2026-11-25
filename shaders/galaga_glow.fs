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
    vec2 px = 1.0 / resolution;
    vec3 base = texture(texture0, uv).rgb;
    vec3 halo = vec3(0.0);
    for (int i = 0; i < 12; i++) {
        float angle = 6.2831853 * float(i) / 12.0;
        vec2 direction = vec2(cos(angle), sin(angle));
        vec3 nearSample = texture(texture0, uv + direction * px * 4.0).rgb;
        vec3 farSample = texture(texture0, uv + direction * px * 11.0).rgb;
        halo += max(nearSample - vec3(0.34), vec3(0.0)) * 0.65;
        halo += max(farSample - vec3(0.44), vec3(0.0)) * 0.35;
    }
    halo /= 12.0;
    float scan = 0.5 + 0.5 * cos(uv.y * resolution.y * 3.14159265);
    float vignette = 1.0 - 0.18 * smoothstep(0.32, 0.82, length((uv - 0.5) * vec2(1.0, 0.9)));
    float sweep = exp(-pow((uv.y - fract(time * 0.035)) * 70.0, 2.0));
    vec3 processed = base * (1.0 - scan * 0.055) * vignette;
    processed += halo * (0.58 + 0.12 * accent);
    processed += accent * sweep * 0.008;
    finalColor = vec4(mix(base, clamp(processed, 0.0, 1.0), clamp(strength, 0.0, 1.0)), 1.0) * fragColor;
}
