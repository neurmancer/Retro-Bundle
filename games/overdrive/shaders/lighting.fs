#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 worldNormal;
in vec3 worldPosition;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float roadScroll;
out vec4 finalColor;
void main()
{
    vec4 base = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(worldNormal);
    float sunset = max(dot(normal, normalize(vec3(-0.25, 0.8, -0.55))), 0.0);
    float rim = max(dot(normal, normalize(vec3(0.85, 0.2, 0.35))), 0.0);
    vec3 illumination = vec3(0.42, 0.39, 0.53) + sunset * vec3(0.50, 0.32, 0.30) + rim * vec3(0.08, 0.19, 0.28);
    float lampX = worldPosition.x < 0.0 ? -10.43 : 10.43;
    float lampZ = roadScroll + 16.0 * floor((worldPosition.z - roadScroll) / 16.0 + 0.5);
    vec3 toLamp = vec3(lampX, 2.77, lampZ) - worldPosition;
    float lampDistance = length(toLamp);
    float lampFalloff = 1.0 - smoothstep(1.0, 8.5, lampDistance);
    float lampFacing = max(dot(normal, normalize(toLamp)), 0.0);
    illumination += vec3(0.18, 0.58, 0.72) * lampFalloff * lampFalloff * (0.4 + 0.6 * lampFacing);
    float fog = smoothstep(100.0, 280.0, abs(worldPosition.z));
    vec3 color = mix(base.rgb * illumination, vec3(0.17, 0.09, 0.24), fog * 0.55);
    finalColor = vec4(color, base.a);
}
