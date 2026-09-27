#include "io.h"
#include "graphics/cp/cp.h"
#include "graphics/gpu/render/backend/software/framebuffer.h"
#include "graphics/gpu/render/backend/software/rasterize/rasterize.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"

#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <sys/_pthread/_pthread_cond_t.h>
#include <sys/_pthread/_pthread_mutex_t.h>

#include <raylib.h>
#include <rlgl.h>

static pthread_mutex_t cond_lock;
static pthread_cond_t cond;

static bool should_draw = false;

static void init_io(void)
{
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&cond_lock, NULL);
}

void trigger_frame(void)
{
    pthread_mutex_lock(&cond_lock);
    should_draw = true;
    pthread_cond_signal(&cond);

    pthread_mutex_unlock(&cond_lock);
}

void io_thread(CPU *cpu)
{
    init_io();

    InitWindow(XFB_WIDTH, XFB_HEIGHT, "render test");
    rlDisableBackfaceCulling();

    Image img = {.data = cpu->bus->xfb,
                 .width = XFB_WIDTH,
                 .height = XFB_HEIGHT,
                 .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D tex = LoadTextureFromImage(img);

    while (!WindowShouldClose()) {
        struct timespec ts;

        clock_gettime(CLOCK_REALTIME, &ts);

        ts.tv_nsec += 16 * 1000 * 1000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }

        pthread_mutex_lock(&cond_lock);
        while (!should_draw) {
            if (pthread_cond_timedwait(&cond, &cond_lock, &ts) != 0) {
                break;
            }
        }
        should_draw = false;
        pthread_mutex_unlock(&cond_lock);

        lock_xfb();
        UpdateTexture(tex, cpu->bus->xfb);
        unlock_xfb();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(tex, 0, 0, WHITE);
        EndDrawing();
    }

    UnloadTexture(tex);
    CloseWindow();
}
