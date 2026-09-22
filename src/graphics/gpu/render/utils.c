#include "render.h"

SDL_GPUShader *LoadShader(SDL_GPUDevice *dev, const char *path, SDL_GPUShaderStage stage)
{
    size_t size;
    void *code = SDL_LoadFile(path, &size);
    SDL_GPUShaderCreateInfo info = {
        .code = code,
        .code_size = size,
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = stage,
    };
    SDL_GPUShader *s = SDL_CreateGPUShader(dev, &info);
    SDL_free(code);
    return s;
}
