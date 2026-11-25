#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform vec2 quadSize;
uniform vec2 cardHalfSize;
uniform float glowStrength;

void main()
{
    float cornerRadius = 8.0;
    vec2 point = abs((fragTexCoord - 0.5) * quadSize) - cardHalfSize + cornerRadius;
    float distanceToEdge = length(max(point, 0.0)) + min(max(point.x, point.y), 0.0) - cornerRadius;
    float outside = max(distanceToEdge, 0.0);
    float halo = exp(-outside / 9.0) * (1.0 - smoothstep(23.0, 30.0, outside));
    finalColor = vec4(fragColor.rgb, fragColor.a * glowStrength * halo);
}
