#include "render.h"
#include "core/config/config.h"
#include "graphics/gpu/vertex/vertex_loader.h"
#include "utils/vector.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static SDL_Window *window;
static SDL_GPUDevice *device;

static Vector vertex_vector;
static BPRegisters bp;
static CPRegisters cp;
static XFRegisters xf;

static u8 _get_vertices_count_for_primitive_type(PrimitiveType t)
{
    switch (t) {
    case GX_QUADS:
    case GX_QUADS_2:
        return 4;

    case GX_TRIANGLES:
        return 3;

    case GX_TRIANGLESTRIP:
    case GX_TRIANGLEFAN:
        return 3;

    case GX_LINES:
        return 2;

    case GX_LINESTRIP:
        return 2;

    case GX_POINTS:
        return 1;

    default:
        assert(!"type unknown \n");
    }
}

void prepare_vertex(Vertex *v)
{
}

static void render_current_state(void)
{
    RENDER_PRINT("[Render] : num of Vertexes : %d\n", vertex_vector.num_of_bytes / sizeof(Vertex));
}

void init_renderer(void)
{
    renderer_load_shaders();

    init_Vector(&vertex_vector, INITIAL_VERTEX_BUFFER_CAP);

    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("gc emu", 620, 480, 0);
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    SDL_ClaimWindowForGPUDevice(device, window);

    SDL_GPUVertexBufferDescription vbDesc = {
        .slot = 0,
        .pitch = sizeof(GpuVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };

    SDL_GPUVertexAttribute attrs[] = {
        {.location = 0,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
         .offset = offsetof(GpuVertex, pos.pos)},
        {.location = 1,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
         .offset = offsetof(GpuVertex, pos.is_3d)},
    };

    SDL_GPUColorTargetDescription colorDesc = {
        .format = SDL_GetGPUSwapchainTextureFormat(device, window),
    };

    SDL_GPUGraphicsPipelineCreateInfo pipeInfo = {
        .vertex_shader =
            LoadShader(device, "./build/shader/shader.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX),
        .fragment_shader =
            LoadShader(device, "./build/shader/shader.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT),
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_input_state =
            {
                .vertex_buffer_descriptions = &vbDesc,
                .num_vertex_buffers = 1,
                .vertex_attributes = attrs,
                .num_vertex_attributes = 2,
            },
        .target_info =
            {
                .color_target_descriptions = &colorDesc,
                .num_color_targets = 1,
            },
    };
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeInfo);
}

void renderer_load_shaders(void)
{
}

void push_vertex_to_vertex_buffer(Vertex v)
{
    if ((vertex_vector.cap_in_bytes - vertex_vector.num_of_bytes) < sizeof(Vertex)) {
        vertex_vector.grow(&vertex_vector);
    }

    memcpy(((u8 *)vertex_vector.buffer) + vertex_vector.num_of_bytes, &v, sizeof(Vertex));
    vertex_vector.num_of_bytes += sizeof(Vertex);
}

void cpy_reg_state(u32 *bp_s, u32 *cp_s, u32 *xf_s)
{
    memcpy(&bp, bp_s, sizeof(bp));
    memcpy(&cp, cp_s, sizeof(cp));
    memcpy(&xf, xf_s, sizeof(xf));

    render_current_state();

    //... render

    vertex_vector.num_of_bytes = 0;
}
