#include "io.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"

#include "scheduler/scheduler.h"
#include "bus/interfaces/si.h"
#include "graphics/gpu/render/backend/software/framebuffer.h"
#include "graphics/gpu/render/backend/software/rasterize/rasterize.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"

#include <_abort.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <sys/_pthread/_pthread_cond_t.h>
#include <sys/_pthread/_pthread_mutex_t.h>

#include <raylib.h>
#include <rlgl.h>

static pthread_mutex_t cond_lock;
static pthread_cond_t cond;

static bool should_draw = false;

static struct timespec t0;

static void init_io(void)
{
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&cond_lock, NULL);
    clock_gettime(CLOCK_MONOTONIC, &t0);
}

void trigger_frame(void)
{
    pthread_mutex_lock(&cond_lock);
    should_draw = true;
    pthread_cond_signal(&cond);

    pthread_mutex_unlock(&cond_lock);
}

static u64 fps = 0;
static u64 cycle_counter_local = 0;

static void _output_info(CPU *cpu)
{
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double s = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    if (s < 1.0)
        return;

    const u64 cycles = cpu->cpu_cycles;
    const u64 frames = get_frames_of_runtime();
    double hz = (double)(cycles - cycle_counter_local) / s;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "avg fps :  %.0f, clock speed : %.0f MHz",
             (double)(frames - fps) / s, hz / 1e6);
    SetWindowTitle(buffer);

    t0 = t1;
    fps = frames;
    cycle_counter_local = cycles;
}

static void test_input(void)
{

    u8 dpad = 0;
    if (IsKeyPressed(KEY_W)) {
        dpad |= BIT(3);
    }
    if (IsKeyPressed(KEY_S)) {
        dpad |= BIT(2);
    }
    if (IsKeyPressed(KEY_D)) {
        dpad |= BIT(1);
    }
    if (IsKeyPressed(KEY_A)) {
        dpad |= BIT(0);
    }
    recieve_input(dpad);
}

void io_thread(CPU *cpu)
{
#ifdef OUTPUT_ENABLE

    init_io();

    InitWindow(XFB_WIDTH, XFB_HEIGHT, "render test");
    rlDisableBackfaceCulling();
    SetTraceLogLevel(LOG_NONE);

    Image img = {.data = cpu->bus->xfb,
                 .width = XFB_WIDTH,
                 .height = XFB_HEIGHT,
                 .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D tex = LoadTextureFromImage(img);

    while (!WindowShouldClose()) {

        SetTraceLogLevel(LOG_NONE);
        struct timespec ts;

        clock_gettime(CLOCK_REALTIME, &ts);

        ts.tv_nsec += 16 * 1000 * 1000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }

        test_input();
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

        _output_info(cpu);
    }

    UnloadTexture(tex);
    CloseWindow();

    abort();
#endif /* ifdef OUTPUT_ENABLE */
}
