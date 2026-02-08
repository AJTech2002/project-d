#[compute]
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) uniform writeonly image2D out_img;

void main() {
	ivec2 px = ivec2(gl_GlobalInvocationID.xy);
	// Simple moving gradient-ish pattern (no time yet)
	vec4 c = vec4(float(px.x) / 1024.0, float(px.y) / 1024.0, 0.2, 1.0);

    if (px.x % 5 == 0 || px.y % 5 == 0) {
	    imageStore(out_img, px, c);
    }
    else {
        imageStore(out_img, px, vec4(0));
    }
}