#include "shader.h"
#include "core/config/config.h"
#include <_abort.h>
#include <_locale.h>
#include <stdio.h>
#include <stdlib.h>

char *load_shader(char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        ERROR("shader at path not found");
        abort();
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc((size_t)size);
    if (!buffer) {
        fclose(f);
        abort();
    }

    fread(buffer, 1, (size_t)size, f);

    fclose(f);

    return buffer;
}

void free_shader(char *shader)
{
    free(shader);
}
