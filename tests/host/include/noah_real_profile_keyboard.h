#pragma once

// Profile serialization tests opt into the production matrix. Other engine
// harnesses retain their synthetic geometry across every translation unit.
#ifdef NOAH_HOST_PRODUCTION_MATRIX

// Production 10 x 6 matrix, including the four unwired positions.
#define MATRIX_ROWS 10
#define MATRIX_COLS 6
#include "qmk_stub.h"

#define LAYOUT(k00, k01, k02, k03, k04, k05, k06, k07, k08, k09, k10, k11, k12, k13, k14, k15, k16, k17, k18, k19, k20, k21, k22, k23, k24, k25, k26, k27, k28, k29, k30, k31, k32, k33, k34, k35, k36, k37, k38, k39, k40, k41, k42, k43, k44, k45, k46, k47, k48, k49, k50, k51, k52, k53, k54, k55) \
    { \
        {k00, k01, k02, k03, k04, k05}, \
        {k12, k13, k14, k15, k16, k17}, \
        {k24, k25, k26, k27, k28, k29}, \
        {k36, k37, k38, k39, k40, k41}, \
        {KC_NO, k50, k54, k48, k49, k53}, \
        {k11, k10, k09, k08, k07, k06}, \
        {k23, k22, k21, k20, k19, k18}, \
        {k35, k34, k33, k32, k31, k30}, \
        {k47, k46, k45, k44, k43, k42}, \
        {KC_NO, k51, KC_NO, k52, KC_NO, k55}, \
    }

#else

#include "qmk_stub.h"

// Host-side matrix shape for the 56-key authored LAYOUT(...). The physical
// split geometry is irrelevant for validation; we only need every authored
// keycode to occupy a stable matrix slot so keymaps[][] can be introspected.
#define LAYOUT(k00, k01, k02, k03, k04, k05, k06, k07, k08, k09, k10, k11, k12, k13, k14, k15, k16, k17, k18, k19, k20, k21, k22, k23, k24, k25, k26, k27, k28, k29, k30, k31, k32, k33, k34, k35, k36, k37, k38, k39, k40, k41, k42, k43, k44, k45, k46, k47, k48, k49, k50, k51, k52, k53, k54, k55)                                                                  \
    {                                                                                                                                                                                                                                                                                                                                                                   \
        {k00, k01, k02, k03, k04, k05, k06, k07}, {k08, k09, k10, k11, k12, k13, k14, k15}, {k16, k17, k18, k19, k20, k21, k22, k23}, {k24, k25, k26, k27, k28, k29, k30, k31}, {k32, k33, k34, k35, k36, k37, k38, k39}, {k40, k41, k42, k43, k44, k45, k46, k47}, {k48, k49, k50, k51, k52, k53, k54, k55}, {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO}, \
    }

#endif
