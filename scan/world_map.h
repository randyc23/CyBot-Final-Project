/*
 * world_map.h
 *
 * 2D map of the environment relative to the bot's starting pose.
 *
 * World frame:
 *   +y = bot's initial forward direction
 *   +x = bot's initial right direction
 *
 * Heading theta:
 *   theta =  0     -> facing +y (forward)
 *   theta = +pi/2  -> facing -x (turned left 90 degrees)
 *   theta = -pi/2  -> facing +x (turned right 90 degrees)
 *   CCW positive (left turn = positive angle), wrapped to (-pi, pi].
 *
 *  Created on: May 4, 2026
 *      Author: randyc1
 */

#ifndef WORLD_MAP_H_
#define WORLD_MAP_H_

#include <stdint.h>
#include "open_interface.h"
#include "boundary_detection.h"

#define MAX_OBJECTS    32
#define MAX_BOUNDARIES 64

typedef struct {
    float x;       // mm, world frame (right of start = positive)
    float y;       // mm, world frame (forward from start = positive)
    float theta;   // radians, CCW positive
} pose_t;

typedef struct {
    float x;
    float y;
    float width_cm;
    float distance_cm;
    uint8_t id;
} world_object_t;

typedef enum {
    KIND_WALL,
    KIND_HOLE
} boundary_kind_t;

typedef struct {
    float x;
    float y;
    boundary_kind_t kind;
} world_boundary_t;

extern pose_t bot_pose;

void world_map_init(void);
void world_map_reset(void);

void pose_update(oi_t *sensor_data);

// Body-to-world transform.
// Body frame: +x = right, +y = forward (matching world frame at theta=0).
void body_to_world(float lx_mm, float ly_mm, float *wx_mm, float *wy_mm);

void world_map_observe(uint8_t scan_angle_deg, float ping_distance_cm);
void world_map_finalize_scan(void);

void world_map_log_boundary(boundary_status_t status);

void world_map_print_pose(void);
void world_map_print(void);
void world_map_send_position(void);

#endif /* WORLD_MAP_H_ */
