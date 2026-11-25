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
    vec2 px = 1.0 / resolution;
    vec3 base = texture(texture0, fragTexCoord).rgb;
    vec3 halo = vec3(0.0);
    halo += max(texture(texture0, fragTexCoord + vec2(4.0, 0.0) * px).rgb - 0.65, 0.0);
    halo += max(texture(texture0, fragTexCoord - vec2(4.0, 0.0) * px).rgb - 0.65, 0.0);
    halo += max(texture(texture0, fragTexCoord + vec2(0.0, 4.0) * px).rgb - 0.65, 0.0);
    halo += max(texture(texture0, fragTexCoord - vec2(0.0, 4.0) * px).rgb - 0.65, 0.0);
    float pulse = 0.95 + 0.05 * sin(time * 0.7);
    vec3 glow = halo * 0.035 * pulse * (vec3(1.0) + accent * 0.2);
    finalColor = vec4(clamp(base + strength * glow, 0.0, 1.0), 1.0) * fragColor;
}
