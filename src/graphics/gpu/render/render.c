#include "render.h"

void init_vertex_buffer(VertexBuffer *self)
{
    self->cap = INITIAL_VERTEX_BUFFER_CAP;
    self->num = 0;
}
