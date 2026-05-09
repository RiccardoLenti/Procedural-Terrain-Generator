#version 330 core

in vec3 vWorldPos;
in vec3 vNormal; 

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uCamPos;

uniform sampler2D uTexGrass;
uniform sampler2D uTexRock;
uniform sampler2D uTexSnow;

void main() {
    float scale = 0.05;
    vec2 uv = vWorldPos.xz * scale;

    vec3 grass = texture(uTexGrass, uv).rgb;
    vec3 rock = texture(uTexRock, uv).rgb;
    vec3 snow = texture(uTexSnow, uv).rgb;

    float y = vWorldPos.y;
    vec3 color;
    if(y < 30.0)
        color = grass; 
    else if (y < 53.0)
        color = mix(grass, rock, (y - 30.0) / 23.0);
    else if (y < 60.0)
        color = rock;
    else if (y < 75.0)
        color = mix(rock, snow, (y - 60.0) / 15.0);
    else 
        color = snow;

    // Phong lighting
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);

    vec3 ambient = 0.15 * color;
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color;

    vec3 viewDir = normalize(uCamPos - vWorldPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.3 * spec * vec3(1.0);

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
