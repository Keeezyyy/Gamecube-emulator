#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i16 int16_t
#define i32 int32_t

#define s8 int8_t
#define s16 int16_t
#define s32 int32_t
#define s64 int64_t

#define f32 float

#define U8_MAX UINT8_MAX
#define U16_MAX UINT16_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX

#define S8_MIN INT8_MIN
#define S8_MAX INT8_MAX
#define S16_MIN INT16_MIN
#define S16_MAX INT16_MAX
#define S32_MIN INT32_MIN
#define S32_MAX INT32_MAX
#define S64_MIN INT64_MIN
#define S64_MAX INT64_MAX

#define NULL_PTR ((void *)0)

#define KB(x) ((size_t)(x) * 1024ULL)
#define MB(x) ((size_t)(x) * 1024ULL * 1024ULL)

#define BIT(n) (1ULL << (n))
#define BIT_SET(x, n) ((x) |= BIT(n))
#define BIT_CLEAR(x, n) ((x) &= ~BIT(n))
#define BIT_TOGGLE(x, n) ((x) ^= BIT(n))
#define BIT_CHECK(x, n) (((x) & BIT(n)) != 0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ROUND_UP(x, n) (((x) + (n) - 1) / (n) * (n))
#define FLOOR_TO(x, n) ((x) / (n) * (n))

#define PACKED __attribute__((__packed__))

#if defined(_MSC_VER)
#define ALIGNAS(n) __declspec(align(n))
#define ALIGNOF(type) __alignof(type)
#elif defined(__GNUC__) || defined(__clang__)
#define ALIGNAS(n) __attribute__((aligned(n)))
#define ALIGNOF(type) __alignof__(type)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#define ALIGNAS(n) _Alignas(n)
#define ALIGNOF(type) _Alignof(type)
#else
#define ALIGNAS(n)
#define ALIGNOF(type) sizeof(type)
#endif

#define UNUSED(x) ((void)(x))
