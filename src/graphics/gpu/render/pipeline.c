#include "pipeline.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"

void load_vertex_into_pipeline(CPU *cpu, Vertex v)
{

    // XF TRANSFORM
#ifdef SOFTWARE_TRANSFORM
    transform_vertex(cpu, &v);
#endif
}
