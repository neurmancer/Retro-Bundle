#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float suitPalette;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    float lightness = dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722));
    float illumination = 0.66 + 0.18 * (1.0 - fragTexCoord.y) + 0.16 * (1.0 - fragTexCoord.x);
    vec3 shadow = mix(vec3(0.36, 0.02, 0.22), vec3(0.02, 0.20, 0.45), suitPalette);
    vec3 highlight = mix(vec3(1.0, 0.46, 0.77), vec3(0.50, 0.94, 1.0), suitPalette);
    vec3 color = mix(shadow, highlight, clamp(lightness * illumination, 0.0, 1.0));
    finalColor = vec4(color * fragColor.rgb, texel.a * fragColor.a);
}
