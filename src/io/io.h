#pragma once

#include "bus/bus.h"
#include "core/config/config.h"
#include <assert.h>

#define BIT_ERRSTAT (1u << 7)
#define BIT_ERRLATCH (1u << 6)
#define BIT_GETORIGIN (1u << 5)
#define BIT_START (1u << 4)
#define BIT_Y (1u << 3)
#define BIT_X (1u << 2)
#define BIT_B (1u << 1)
#define BIT_A (1u << 0)

#define BIT_USEORIGIN (1u << 7)
#define BIT_L_DIGITAL (1u << 6)
#define BIT_R_DIGITAL (1u << 5)
#define BIT_Z (1u << 4)
#define BIT_D_UP (1u << 3)
#define BIT_D_DOWN (1u << 2)
#define BIT_D_RIGHT (1u << 1)
#define BIT_D_LEFT (1u << 0)

typedef struct {
    u8 btn_1;
    u8 btn_2;

    u8 stick_x;
    u8 stick_y;

    u8 c_stick_x;
    u8 c_stick_y;

    u8 l_analog;
    u8 r_analog;

} PACKED ControllerInput;

static_assert(sizeof(ControllerInput) == 8);

void io_thread(CPU *cpu);

void trigger_frame(void);
