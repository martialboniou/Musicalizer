#version 100

// for OpenGL ES 2.0 (using Google Chrome's Angle)

precision mediump float;

// input vertex attributs (from vertex shader)
varying vec2 fragTexCoord;
varying vec4 fragColor;

// gl_FragColor is our output fragment color

void main() {
    float r = 0.1;
    vec2 p = fragTexCoord - vec2(0.5);
    float len = length(p);
    if (len <= 0.5) {
        float s = len - r;
        if (s <= 0.0) {
            gl_FragColor = fragColor;
        } else {
            float t = 1.0 - s / (0.5 - r);
            gl_FragColor = vec4(fragColor.xyz, t * t * t * t);
        }
    } else {
        gl_FragColor = vec4(0);
    }
}

/** original dump (from https://github.com/tsoding)
#version 330

in vec2 fragTexCoord;
in vec4 fragColor

out vec4 finalColor;

void main() {
    float r = 0.1;
    vec2 p = fragTexCoord - vec2(0.5);
    if (length(p) <= 0.5) {
        float s = length(p) - r;
        if (s <= 0) {
            finalColor = fragColor;
        } else {
            float t = 1 - s / (0.5 - r);
            finalColor = vec4(fragColor.xyz, t * t * t * t);
        }
    } else {
        finalColor = vec4(0);
    }
}
*/
