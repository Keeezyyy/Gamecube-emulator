#include "io.h"
#include "core/config/config.h"
#include "graphics/cp/cp.h"

#include "scheduler/scheduler.h"
#include "bus/interfaces/si.h"
#include "graphics/gpu/render/backend/software/framebuffer.h"
#include "graphics/gpu/render/backend/software/rasterize/rasterize.h"
#include "graphics/gpu/render/backend/software/transform/transform.h"

#include <_abort.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
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

static bool is_set = false;
static float lX, lY, rX, rY;

static ControllerInput last_state;

#define DEADZONE 0.15f

static void test_input(void)
{

    ControllerInput i = {0};

    float leftX = (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) + 1) / 2;
    float leftY = (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) + 1) / 2;

    float rightX = (GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) + 1) / 2;
    float rightY = (GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) + 1) / 2;

    float lt = (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_TRIGGER) + 1) / 2;
    float rt = (GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER) + 1) / 2;

    if (IsKeyDown(KEY_A))
        leftX = 0.0f;
    if (IsKeyDown(KEY_D))
        leftX = 1.0f;
    if (IsKeyDown(KEY_W))
        leftY = 0.0f;
    if (IsKeyDown(KEY_S))
        leftY = 1.0f;

    if (IsKeyDown(KEY_LEFT))
        rightX = 0.0f;
    if (IsKeyDown(KEY_RIGHT))
        rightX = 1.0f;
    if (IsKeyDown(KEY_UP))
        rightY = 0.0f;
    if (IsKeyDown(KEY_DOWN))
        rightY = 1.0f;

    if (IsKeyDown(KEY_Q))
        i.btn_2 |= BIT_Z;

    if (IsKeyDown(KEY_DOWN))
        i.btn_2 |= BIT_D_DOWN;
    if (IsKeyDown(KEY_LEFT))
        i.btn_2 |= BIT_D_LEFT;
    if (IsKeyDown(KEY_RIGHT))
        i.btn_2 |= BIT_D_RIGHT;
    if (IsKeyDown(KEY_UP))
        i.btn_2 |= BIT_D_UP;

    if (IsKeyDown(KEY_J))
        i.btn_1 |= BIT_A;
    if (IsKeyDown(KEY_K))
        i.btn_1 |= BIT_B;
    if (IsKeyDown(KEY_L))
        i.btn_1 |= BIT_X;
    if (IsKeyDown(KEY_I))
        i.btn_1 |= BIT_Y;

    if (fabsf(leftX) < DEADZONE)
        leftX = 0.0f;
    if (fabsf(leftY) < DEADZONE)
        leftY = 0.0f;

    if (fabsf(rightX) < DEADZONE)
        rightX = 0.0f;
    if (fabsf(rightY) < DEADZONE)
        rightY = 0.0f;

    if (fabsf(lt) < DEADZONE)
        lt = 0.0f;
    if (fabsf(rt) < DEADZONE)
        rt = 0.0f;

    i.stick_x = (u8)(leftX * 255);
    i.stick_y = (u8)(255 - leftY * 255);

    i.c_stick_x = (u8)(rightX * 255);
    i.c_stick_y = (u8)(255 - rightY * 255);

    i.l_analog = (u8)(lt * 255);
    i.r_analog = (u8)(rt * 255);

    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1))
        i.btn_2 |= BIT_Z;

    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        i.btn_2 |= BIT_D_DOWN;
    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
        i.btn_2 |= BIT_D_LEFT;
    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
        i.btn_2 |= BIT_D_RIGHT;
    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP))
        i.btn_2 |= BIT_D_UP;

    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        i.btn_1 |= BIT_A;
    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
        i.btn_1 |= BIT_B;
    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
        i.btn_1 |= BIT_X;
    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP))
        i.btn_1 |= BIT_Y;
    if (memcmp(&i, &last_state, sizeof(ControllerInput)) == 0) {
        return;
    }
    memcpy(&last_state, &i, sizeof(ControllerInput));

    recieve_input(i);
}

#define OUTPUT_ENABLE
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
