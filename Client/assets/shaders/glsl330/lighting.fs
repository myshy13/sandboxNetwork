#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// reflection 0.0 matte, 1.0 strong reflection
uniform float reflectivity;

// lighting and shadows
uniform mat4 lightVP;
uniform sampler2D shadowMap;
uniform int shadowMapResolution;
uniform int useShadows;  // 0 = skip the lookup (setting off)

// Output fragment color
#ifdef GL_ES
#define finalColor gl_FragColor
#else
out vec4 finalColor;
#endif

// NOTE: Add here your custom variables

#define     MAX_LIGHTS              4
#define     LIGHT_DIRECTIONAL       0
#define     LIGHT_POINT             1

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

// Input lighting values
uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
  // Texel color fetching from texture sampler
  vec4 texelColor = texture(texture0, fragTexCoord);
  if (texelColor.a < 0.01) discard;  // cutout: glass's empty interior writes nothing
  texelColor.rgb = pow(texelColor.rgb, vec3(1.6));
  vec3 lightDot = vec3(0.0);
  vec3 normal = normalize(fragNormal);
  vec3 viewD = normalize(viewPos - fragPosition);
  vec3 specular = vec3(0.0);

  vec4 tint = colDiffuse * fragColor;

  vec4 p = lightVP * vec4(fragPosition, 1);
  p.xyz /= p.w;
  p.xyz = (p.xyz + 1.0)/2.0;
  vec2 sampleCoords = p.xy;
  float currentDepth = p.z;

  vec2 texelSize = vec2(1.0 / float(shadowMapResolution));

  // Slope-scaled bias: the flatter the sun hits a surface, the more the map's
  // depth changes across one texel (tan of the angle to the normal), so the
  // bias grows with it. 0.0005 is about one texel's depth per unit of slope.
  vec3 sunDir = normalize(lights[0].position - lights[0].target);  // toward the sun
  float cosTheta = clamp(dot(normal, sunDir), 0.05, 1.0);
  float tanTheta = sqrt(1.0 - cosTheta*cosTheta) / cosTheta;
  float bias = clamp(0.0005 * tanTheta, 0.0001, 0.005);
  
  // 1.0 = fully lit; stays 1.0 when the shadows setting is off.
  float lit = 1.0;
  if (useShadows != 0) {
    int blockedSamples = 0;
    for (int x = -1; x <= 1; x++) {
      for (int y = -1; y <= 1; y++) {
        float sampleDepth = texture(shadowMap, sampleCoords + texelSize*vec2(x,y)).r;
        if (currentDepth - bias > sampleDepth) {
          blockedSamples++;
        }
      }
    }
    lit = 1.0 - float(blockedSamples) / 9.0;

    // Beyond the map (or the sun camera's far plane) there is no data: treat as lit.
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0) lit = 1.0;

    // Fade the shadow out over the map's outer tenth, so it doesn't end in a line.
    float edge = max(abs(p.x - 0.5), abs(p.y - 0.5)) * 2.0;  // 0 centre, 1 edge
    lit = mix(lit, 1.0, smoothstep(0.9, 1.0, edge));
  }

  for (int i = 0; i < MAX_LIGHTS; i++)
  {
    if (lights[i].enabled == 1)
    {
      vec3 light = vec3(0.0);

      if (lights[i].type == LIGHT_DIRECTIONAL)
      {
        light = -normalize(lights[i].target - lights[i].position);
      }

      if (lights[i].type == LIGHT_POINT)
      {
        light = normalize(lights[i].position - fragPosition);
      }

      float NdotL = max(dot(normal, light), 0.0);
      lightDot += lights[i].color.rgb*NdotL;

      float specCo = 0.0;
      if (NdotL > 0.0) {
        specCo = pow(max(0.0, dot(viewD, reflect(-(light), normal))), 48.0); // exponent = tightness of the highlight
        specular += specCo*0.15*reflectivity; // overall sheen strength - lower = more matte
      }
    }
  }

  finalColor = (texelColor*(tint*vec4(lightDot, 1.0))) + vec4(specular, 0.0);
  finalColor.rgb *= lit;
  finalColor += texelColor*(ambient/2)*tint;

  // Gamma correction
  finalColor = pow(finalColor, vec4(1.0/2.2));

  // Noise keyed to world position so the grain sticks to surfaces; 0.04 = strength
  finalColor.rgb += (rand(fragPosition.xy + fragPosition.z) - 0.5)*0.04;
}