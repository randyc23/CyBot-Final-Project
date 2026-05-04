/*
 * world_map.h
 *
 *  Created on: May 4, 2026
 *      Author: randyc1
 */

#ifndef WORLD_MAP_H_
#define WORLD_MAP_H_

#include <stdint.h>
#include "open_interface.h"
#include "boundary_detection.h"

#define MAX_OBJECTS 32
#define MAX_BOUNDARIES 64

typedef struct {
    float x;
    float y;
    float theta;
} pose_t;

typedef struct {
    float x;
    float y;
    float width_cm;
    float distance_cm;
    uint8_t id;
} world_object_t;

typedef enum{
    KIND_WALL,
    KIND_HOLE
}boundary_kind_t;

typedef struct {
    float x;
    float y;
    boundary_kind_t kind;
} world_boundary_t;

extern pose_t bot_pose;

// Lifecycle
void world_map_init(void);
void world_map_reset(void); //wipe objects, boundaries

// Pose tracking. Call once per oi_update()
void pose_update(oi_t *sensor_data);

//Body to world
void body_to_world(float lx_mm, float ly_mm, float *wx_mm, float *wy_mm);

//object logging, feed every sample as the scan progresses
void world_map_observe(uint8_t scan_angle_deg, float ping_distance_cm);

//Call once at end of scan to segment and commit changes to the map
void world_map_finalize_scan(void);

//Boundary logging
void world_map_log_boundary(boundary_status_t status);

//Output
void world_map_print_pose(void);
void world_map_print(void);




#endif /* WORLD_MAP_H_ */
