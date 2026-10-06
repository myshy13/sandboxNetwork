#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in mat4 instanceTransform;   // per-instance model matrix
in vec4 instanceColor;

// Input uniform values
uniform mat4 mvp;            // here: projection * view (raylib supplies it this way for instancing)
uniform mat4 matNormal;      // unused per-instance; we compute normals from instanceTransform

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
    // The renderer stores a texture crop in the matrix's spare slot; read it, then clear it.
    mat4 model = instanceTransform;
    float visible = 1.0 - model[0][3];
    model[0][3] = 0.0;

    // Compute per-instance MVP
    mat4 mvpi = mvp * model;

    fragPosition = vec3(model * vec4(vertexPosition, 1.0));
    // Keep the top of the texture and cut the bottom, so a thin water strip isn't squashed.
    fragTexCoord = vec2(vertexTexCoord.x, vertexTexCoord.y * visible);
    fragColor = vertexColor * instanceColor;

    // Normal matrix from the instance transform.
    // Fine for uniform scale; for non-uniform scale you'd want inverse-transpose.
    fragNormal = normalize(vec3(model * vec4(vertexNormal, 0.0)));

    gl_Position = mvpi * vec4(vertexPosition, 1.0);
}