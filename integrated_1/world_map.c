/*
 * world_map.c
 *
 *  Created on: May 4, 2026
 *      Author: randyc1
 */

#include "world_map.h"
#include "uart-interrupt.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

#define DEG2RAD(d) ((float)(d) * (float)M_PI / 180.0f)
#define RAD2DEG(r) ((float)(r) * 180.0f / (float)M_PI)

//Constants of the bots, measure them out

#define SCAN_OFFSET_FWD_MM 80.0f // Figure out this one, scanner offset relative to center of bot
#define SCAN_OFFSET_LAT_MM 0.0f

//Cliff sensor offsets, sensor offset relative to the center of the bot

#define CLIFF_FL_FWD_MM 150.0f
#define CLIFF_FL_LAT_MM 50.0f
#define CLIFF_FR_FWD_MM 150.0f
#define CLIFF_FR_LAT_MM -50.0f
#define CLIFF_L_FWD_MM 120.0f
#define CLIFF_L_LAT_MM 150.0f
#define CLIFF_R_FWD_MM 120.0f
#define CLIFF_R_LAT_MM -150.0f
#define CLIFF_FRONT_FWD_MM 150.0f
#define CLIFF_FRONT_LAT_MM 0.0f

// Object segmentation
#define OBJECT_MIN_DIST_CM      5.0f
#define OBJECT_MAX_DIST_CM    150.0f
#define OBJECT_GAP_CM          15.0f   // distance jump that ends an object
#define OBJECT_MIN_SAMPLES        2    // ignore single-sample blips
#define DEDUP_RADIUS_MM       150.0f   // re-observation if within this


//State
pose_t bot_pose = {0.0f, 0.0f, 0.0f};

static world_object_t objects[MAX_OBJECTS];
static uint8_t obj_count = 0;
static world_boundary_t boundaries[MAX_BOUNDARIES];
static uint8_t bnd_count = 0;

#define MAX_SAMPLES 181
typedef struct {
    uint8_t angle_deg;
    float distance_cm;
    uint8_t valid;
} scan_sample_t;
static scan_sample_t scan_buf[MAX_SAMPLES];
static int scan_n = 0;

//Helpers
static float wrap_to_pi(float a){
    while (a > (float)M_PI) a -= 2.0f * (float)M_PI;
    while (a < -(float)M_PI) a += 2.0f * (float)M_PI;
    return a;
}

static void uart_print(const char *s){
    uart_sendStr(s);
}

//Lifecycle
void world_map_init(void){
    bot_pose.x = bot_pose.y = bot_pose.theta = 0.0f;
    obj_count = 0;
    bnd_count = 0;
    scan_n = 0;
}

void world_map_reset(void){
    world_map_init();
}

//Pose tracking

void pose_update(oi_t *sensor_data){

    float dd = (float)sensor_data -> distance;
    float da_rad = DEG2RAD((float)sensor_data->angle);

    float mid_theta = bot_pose.theta + 0.5f * da_rad;
    bot_pose.x += dd * cosf(mid_theta);
    bot_pose.y += dd * sinf(mid_theta);
    bot_pose.theta = wrap_to_pi(bot_pose.theta + da_rad);

}

void body_to_world(float lx, float ly, float *wx, float *wy){
    float c = cosf(bot_pose.theta);
    float s = sinf(bot_pose.theta);
    *wx = bot_pose.x + lx * c - ly * s;
    *wy = bot_pose.y +lx * s + ly * c;
}

//object logging

void world_map_observe(uint8_t scan_angle_deg, float ping_distance_cm){
    if(scan_n >= MAX_SAMPLES) return;
    scan_buf[scan_n].angle_deg = scan_angle_deg;
    scan_buf[scan_n].distance_cm = ping_distance_cm;
    scan_buf[scan_n].valid = (ping_distance_cm > OBJECT_MIN_DIST_CM && ping_distance_cm < OBJECT_MAX_DIST_CM);
    scan_n++;
}

static void commit_object(int start_idx, int end_idx_excl) {
    int n = end_idx_excl - start_idx;
    if (n < OBJECT_MIN_SAMPLES) return;
    if (obj_count >= MAX_OBJECTS) return;

    // Object center = midpoint sample.
    int mid = start_idx + n / 2;
    uint8_t servo_deg = scan_buf[mid].angle_deg;
    float   dist_cm   = scan_buf[mid].distance_cm;

    // Servo angle convention: 0=right, 90=fwd, 180=left.
    // Bot-relative angle alpha (CCW positive = left of forward) = servo - 90.
    float alpha = DEG2RAD((float)servo_deg - 90.0f);
    float dist_mm = dist_cm * 10.0f;

    // Local position (forward = +x, left = +y), accounting for scanner mount.
    float lx = SCAN_OFFSET_FWD_MM + dist_mm * cosf(alpha);
    float ly = SCAN_OFFSET_LAT_MM + dist_mm * sinf(alpha);

    float wx, wy;
    body_to_world(lx, ly, &wx, &wy);

    // Dedup: if within DEDUP_RADIUS_MM of an existing object, skip.
    int i;
    for (i = 0; i < obj_count; i++) {
        float dx = wx - objects[i].x;
        float dy = wy - objects[i].y;
        if (dx*dx + dy*dy < DEDUP_RADIUS_MM * DEDUP_RADIUS_MM) {
            return;
        }
    }

    // Approximate width: 2 * d * tan(angular_width / 2)
    float angular_width_deg = (float)n;  // 1 sample per degree if increment=1
    float width_cm = 2.0f * dist_cm *
                     tanf(DEG2RAD(angular_width_deg / 2.0f));

    objects[obj_count].x          = wx;
    objects[obj_count].y          = wy;
    objects[obj_count].width_cm   = width_cm;
    objects[obj_count].distance_cm = dist_cm;
    objects[obj_count].id         = obj_count;
    obj_count++;
}

void world_map_finalize_scan(void) {
    int in_obj = 0;
    int start  = 0;
    float prev_dist = 0.0f;
    int i;

    for (i = 0; i < scan_n; i++) {
        scan_sample_t *s = &scan_buf[i];

        if (!s->valid) {
            if (in_obj) {
                commit_object(start, i);
                in_obj = 0;
            }
            continue;
        }

        if (!in_obj) {
            in_obj   = 1;
            start    = i;
            prev_dist = s->distance_cm;
        } else {
            if (fabsf(s->distance_cm - prev_dist) > OBJECT_GAP_CM) {
                commit_object(start, i);
                start = i;
            }
            prev_dist = s->distance_cm;
        }
    }
    if (in_obj) commit_object(start, scan_n);

    scan_n = 0;
}

//Boundary logging
static void boundary_offsets(boundary_status_t st, float *fwd, float *lat, boundary_kind_t *kind) {
    switch (st) {
        case BOUNDARY_LEFT:        *fwd = CLIFF_L_FWD_MM;     *lat = CLIFF_L_LAT_MM;     *kind = KIND_WALL; break;
        case BOUNDARY_FRONT_LEFT:  *fwd = CLIFF_FL_FWD_MM;    *lat = CLIFF_FL_LAT_MM;    *kind = KIND_WALL; break;
        case BOUNDARY_FRONT_RIGHT: *fwd = CLIFF_FR_FWD_MM;    *lat = CLIFF_FR_LAT_MM;    *kind = KIND_WALL; break;
        case BOUNDARY_RIGHT:       *fwd = CLIFF_R_FWD_MM;     *lat = CLIFF_R_LAT_MM;     *kind = KIND_WALL; break;
        case BOUNDARY_FRONT:       *fwd = CLIFF_FRONT_FWD_MM; *lat = CLIFF_FRONT_LAT_MM; *kind = KIND_WALL; break;
        case HOLE_LEFT:            *fwd = CLIFF_L_FWD_MM;     *lat = CLIFF_L_LAT_MM;     *kind = KIND_HOLE; break;
        case HOLE_FRONT_LEFT:      *fwd = CLIFF_FL_FWD_MM;    *lat = CLIFF_FL_LAT_MM;    *kind = KIND_HOLE; break;
        case HOLE_FRONT_RIGHT:     *fwd = CLIFF_FR_FWD_MM;    *lat = CLIFF_FR_LAT_MM;    *kind = KIND_HOLE; break;
        case HOLE_RIGHT:           *fwd = CLIFF_R_FWD_MM;     *lat = CLIFF_R_LAT_MM;     *kind = KIND_HOLE; break;
        case HOLE_FRONT:           *fwd = CLIFF_FRONT_FWD_MM; *lat = CLIFF_FRONT_LAT_MM; *kind = KIND_HOLE; break;
        default:                   *fwd = 0; *lat = 0; *kind = KIND_WALL; break;
    }
}

void world_map_log_boundary(boundary_status_t st) {
    if (st == BOUNDARY_CLEAR) return;
    if (bnd_count >= MAX_BOUNDARIES) return;

    float fwd, lat;
    boundary_kind_t kind;
    boundary_offsets(st, &fwd, &lat, &kind);

    float wx, wy;
    body_to_world(fwd, lat, &wx, &wy);

    // Dedup within 100 mm
    int i;
    for (i = 0; i < bnd_count; i++) {
        float dx = wx - boundaries[i].x;
        float dy = wy - boundaries[i].y;
        if (dx*dx + dy*dy < 100.0f * 100.0f &&
            boundaries[i].kind == kind) {
            return;
        }
    }
    boundaries[bnd_count].x    = wx;
    boundaries[bnd_count].y    = wy;
    boundaries[bnd_count].kind = kind;
    bnd_count++;
}

//Output

void world_map_print_pose(void) {
    char line[96];
    sprintf(line, "[POSE] x=%.1f y=%.1f theta=%.1f deg\r\n",
            bot_pose.x, bot_pose.y, RAD2DEG(bot_pose.theta));
    uart_print(line);
}

void world_map_print(void) {
    char line[128];

    uart_print("\r\n=== MAP DUMP ===\r\n");
    sprintf(line, "Pose: x=%.1f mm  y=%.1f mm  theta=%.1f deg\r\n",
            bot_pose.x, bot_pose.y, RAD2DEG(bot_pose.theta));
    uart_print(line);

    sprintf(line, "Objects (%d):\r\n", obj_count);
    uart_print(line);
    int i;
    for (i = 0; i < obj_count; i++) {
        sprintf(line, "  [%d] x=%.1f y=%.1f width=%.1fcm dist=%.1fcm\r\n",
                objects[i].id, objects[i].x, objects[i].y,
                objects[i].width_cm, objects[i].distance_cm);
        uart_print(line);
    }

    sprintf(line, "Boundaries (%d):\r\n", bnd_count);
    uart_print(line);
    for (i = 0; i < bnd_count; i++) {
        const char *kindstr = (boundaries[i].kind == KIND_HOLE) ? "HOLE" : "WALL";
        sprintf(line, "  [%d] %s at x=%.1f y=%.1f\r\n",
                i, kindstr, boundaries[i].x, boundaries[i].y);
        uart_print(line);
    }

    uart_print("=== END ===\r\n\r\n");
}

