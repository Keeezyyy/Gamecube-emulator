#include "pipeline.h"
#include "core/config/config.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"
#include "graphics/gpu/vertex/primitive.h"

void load_vertex_into_pipeline(CPU *cpu, Primitive p)
{

    // TODO: find better alternative maybe gloabl dynamic array
    XFOutput xf_output_buffer[U16_MAX] = {0};

    for (int i = 0; i < p.vert_count; i++) {
        // XF TRANSFORM
#ifdef SOFTWARE_TRANSFORM
        xf_output_buffer[i] = transform_vertex(cpu, &p.vertecies[i]);
#endif
    }
}
