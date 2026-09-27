#include "io.h"
#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/_pthread/_pthread_cond_t.h>
#include <sys/_pthread/_pthread_mutex_t.h>

static pthread_mutex_t cond_lock;
static pthread_cond_t cond;

static bool should_draw = false;

void init_io(void)
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

void displa_thread(CPU *cpu)
{
    bool running = true;
    while (running) {
        SDL_Event e;

        pthread_mutex_lock(&cond_lock);

        while (!should_draw) {
            pthread_cond_wait(&cond, &cond_lock);
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT)
                    running = false;
            }
        }

        should_draw = false;
        pthread_mutex_unlock(&cond_lock);

        /*
            SDL_UpdateTexture(texture, NULL, framebuffer, fb_width * sizeof(uint32_t));

            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        */
    }
}
