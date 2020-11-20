#pragma once

#include "HandmadeMath.h"

bool app_init    ();
void app_shutdown();
void app_render  (float t, hmm_mat4 view, hmm_mat4 proj);