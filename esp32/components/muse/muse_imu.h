/*
 * Motion reactions for Muse (community addition): tilt or slide the board and Muse looks that
 * way; pick it up or shake it and Muse is startled. Needs muse_board_t.imu_read.
 */

#pragma once

#include "muse_pixel.h"

/* Fills the pose's look_*, lean and jolt from the board's motion sensor. No-op without one. */
void muse_imu_apply(muse_pose_t *pose, float now);

/* Serial commands: ">imu.debug=1|0" logs raw readings and the resulting look. */
bool muse_imu_command(const char *name, const char *value, const char **result);
