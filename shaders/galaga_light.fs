#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform float glowRadius;
uniform float glowStrength;

void main()
{
    float radius = length(fragTexCoord * 2.0 - 1.0);
    float d = radius * glowRadius;
    float halo = glowStrength * (0.7 * exp(-0.3 * d * d) + 0.3 * exp(-1.3 * d * d));
    float center = 1.0 - smoothstep(0.0, 0.45, d);
    float fade = 1.0 - smoothstep(0.75, 1.0, radius);
    finalColor = vec4(fragColor.rgb * (halo + 0.25 * center), fragColor.a * fade);
}
