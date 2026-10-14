#pragma once
#include <stdbool.h>
// Latest userspace sensor x/y sample, before suppression and mode transforms.
// No continuation, noise classification, or generated-output inference.
bool noah_pointing_sensor_motion_observed(void);
