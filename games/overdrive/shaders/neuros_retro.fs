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
    vec3 glow = vec3(0.0);
    glow += texture(texture0, uv + vec2(pixel.x * 2.0, 0.0)).rgb;
    glow += texture(texture0, uv - vec2(pixel.x * 2.0, 0.0)).rgb;
    glow += texture(texture0, uv + vec2(0.0, pixel.y * 2.0)).rgb;
    glow += texture(texture0, uv - vec2(0.0, pixel.y * 2.0)).rgb;
    glow *= 0.25;
    glow = max(glow - vec3(0.48), vec3(0.0));

    float scanline = 0.5 + 0.5 * cos(uv.y * resolution.y * 3.14159265);
    float edge = 1.0 - smoothstep(0.22, 0.95, length((uv - 0.5) * vec2(1.0, 0.85)));
    float sweep = exp(-pow((uv.y - fract(time * 0.045)) * 55.0, 2.0));
    vec3 treated = base * (1.0 - scanline * 0.10);
    treated += glow * (vec3(0.12) + accent * 0.10);
    treated += accent * sweep * 0.012;
    treated *= 0.94 + 0.06 * edge;
    finalColor = vec4(mix(base, clamp(treated, 0.0, 1.0), clamp(strength, 0.0, 1.0)), 1.0) * fragColor;
}
