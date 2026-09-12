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
    // Compute per-instance MVP
    mat4 mvpi = mvp * instanceTransform;

    fragPosition = vec3(instanceTransform * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor * instanceColor;

    // Normal matrix from the instance transform.
    // Fine for uniform scale; for non-uniform scale you'd want inverse-transpose.
    fragNormal = normalize(vec3(instanceTransform * vec4(vertexNormal, 0.0)));

    gl_Position = mvpi * vec4(vertexPosition, 1.0);
}