#include "window.h"
#include <GLFW/glfw3.h>
#include <_abort.h>

static GLFWwindow *window;

void windows_init(void)
{
    if (!glfwInit())
        return;

    window = glfwCreateWindow(640, 480, "gc emu", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return;
    }
}

void window_finish_frame(void)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glfwSwapBuffers(window);
}

void window_thread(void)
{

    glfwMakeContextCurrent(window);

    while (!glfwWindowShouldClose(window)) {

        window_finish_frame();
        glfwPollEvents();
    }
    glfwTerminate();

    abort();
    return;
}
