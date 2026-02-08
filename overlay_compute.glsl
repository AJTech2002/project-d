#[compute]
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) uniform writeonly image2D out_img;

// width and height as uniform
layout(set = 0, binding = 1) uniform Params {
    int screenWidth;
    int screenHeight;
    int gridWidth;
    int gridHeight;
    int camX;
    int camY;
} params;

struct Cell {
    int type; // 0 = empty, 1 = sand
    // padding to 16 bytes for std430 layout
    int padding[3];
};

layout (set = 0, binding = 2) buffer Cells {
    Cell cells[];
};

void main() {
	ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    ivec2 gridPos = px + ivec2(params.camX, params.camY);
    if (px.x >= params.screenWidth || px.y >= params.screenHeight) {
        return; // out of bounds
    }

    if (gridPos.x < 0 || gridPos.y < 0 || gridPos.x >= params.gridWidth || gridPos.y >= params.gridHeight) {
        imageStore(out_img, px, vec4(1.0, 1.0, 1.0, 0.0)); // black for out of bounds
        return;
    }

    // int idx = px.y * params.screenWidth + px.x;
    int idx = gridPos.y * params.gridWidth + gridPos.x;
    Cell cell = cells[idx];

    if (cell.type == 1) { // sand
        imageStore(out_img, px, vec4(1.0, 1.0, 0.0, 1.0)); // yellow for sand
    }
    else {
        imageStore(out_img, px, vec4(0.0, 0.0, 0.0, 0.0)); // black for empty
    }
    // imageStore(out_img, px, vec4(1.0, 0.0, 1.0, 1.0));

}