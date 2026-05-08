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

// ------------------------------------------------------------------
// Tunable constants
// ------------------------------------------------------------------

// Body-frame sensor offsets.
// Body frame: +x = right of bot, +y = forward of bot.
// All offsets measured from the bot's center of rotation.

// Scanner mount
#define SCAN_X_MM               0.0f
#define SCAN_Y_MM              80.0f

// Cliff sensors (Roomba ~175mm radius; front-half mounted)
#define CLIFF_FL_X_MM         -50.0f      // left of centerline
#define CLIFF_FL_Y_MM         150.0f
#define CLIFF_FR_X_MM          50.0f      // right of centerline
#define CLIFF_FR_Y_MM         150.0f
#define CLIFF_L_X_MM         -150.0f      // outer left
#define CLIFF_L_Y_MM          120.0f
#define CLIFF_R_X_MM          150.0f      // outer right
#define CLIFF_R_Y_MM          120.0f
#define CLIFF_FRONT_X_MM        0.0f
#define CLIFF_FRONT_Y_MM      150.0f

// Edge-based object detection
#define OBJECT_MIN_DIST_CM        5.0f
#define OBJECT_MAX_DIST_CM      150.0f
#define EDGE_THRESHOLD_CM         7.0f //For ping
#define EDGE_THRESHOLD_IR         245 //For IR
#define EDGE_LOOKBACK             2
#define OBJECT_MIN_SAMPLES        2
#define DEDUP_RADIUS_MM         150.0f

// Pose tracking: suppress distance integration during fast turns.
// Above this angular velocity per update, the OI's reported distance
// is almost entirely wheel slip rather than real translation, so
// integrating it just produces accumulated position drift.
#define TURN_SUPPRESS_THRESHOLD_DEG  3.0f

// ------------------------------------------------------------------
// State
// ------------------------------------------------------------------

pose_t bot_pose = { 0.0f, 0.0f, 0.0f };

static world_object_t   objects[MAX_OBJECTS];
static uint8_t          obj_count = 0;
static world_boundary_t boundaries[MAX_BOUNDARIES];
static uint8_t          bnd_count = 0;

#define MAX_SAMPLES 181
typedef struct {
    uint8_t angle_deg;
    float   distance_cm;
    uint8_t valid;
} scan_sample_t;
static scan_sample_t scan_buf[MAX_SAMPLES];
static int           scan_n = 0;

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------

static float wrap_to_pi(float a) {
    while (a >  (float)M_PI) a -= 2.0f * (float)M_PI;
    while (a < -(float)M_PI) a += 2.0f * (float)M_PI;
    return a;
}

static void uart_print(const char *s) {
    uart_sendStr(s);
}

// ------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------

void world_map_init(void) {
    bot_pose.x = bot_pose.y = bot_pose.theta = 0.0f;
    obj_count = 0;
    bnd_count = 0;
    scan_n = 0;
}

void world_map_reset(void) {
    world_map_init();
}

// ------------------------------------------------------------------
// Pose tracking
// ------------------------------------------------------------------

void pose_update(oi_t *sensor_data) {
    float dd     = (float)sensor_data->distance;          // mm
    float da_rad = DEG2RAD((float)sensor_data->angle);    // radians

    // Suppress distance during fast turns.
    // The OI reports the average of the two wheels; for an in-place spin
    // the wheels travel equal-and-opposite, so the average should be zero.
    // In practice, mechanical asymmetry produces 1-3 mm/update of residual
    // distance that gets integrated as position drift. If we're spinning
    // faster than the threshold, that residual is noise, not motion.
    if (fabsf(da_rad) > DEG2RAD(TURN_SUPPRESS_THRESHOLD_DEG)) {
        dd = 0.0f;
    }

    // Midpoint integration. theta=0 means the bot faces +y, so the
    // forward direction unit vector is (-sin theta, cos theta).
    float mid_theta = bot_pose.theta + 0.5f * da_rad;
    bot_pose.x    += -dd * sinf(mid_theta);
    bot_pose.y    +=  dd * cosf(mid_theta);
    bot_pose.theta = wrap_to_pi(bot_pose.theta + da_rad);
}

void body_to_world(float lx, float ly, float *wx, float *wy) {
    // Standard 2D rotation: rotate body-frame point by theta, then translate.
    // Same formula as before; only the (lx, ly) interpretation changed.
    float c = cosf(bot_pose.theta);
    float s = sinf(bot_pose.theta);
    *wx = bot_pose.x + lx * c - ly * s;
    *wy = bot_pose.y + lx * s + ly * c;
}

// ------------------------------------------------------------------
// Object detection (edge-based)
// ------------------------------------------------------------------

void world_map_observe(uint8_t scan_angle_deg, float ping_distance_cm) {
    if (scan_n >= MAX_SAMPLES) return;
    scan_buf[scan_n].angle_deg   = scan_angle_deg;
    scan_buf[scan_n].distance_cm = ping_distance_cm;
    //scan_buf[scan_n].valid =(ping_distance_cm > OBJECT_MIN_DIST_CM && ping_distance_cm < OBJECT_MAX_DIST_CM);
    scan_buf[scan_n].valid = 1;
    scan_n++;
}

static void commit_object(int start_idx, int end_idx_excl) {
    int n = end_idx_excl - start_idx;
    if (n < OBJECT_MIN_SAMPLES) return;
    if (obj_count >= MAX_OBJECTS) return;

    uint8_t init_angle_deg = scan_buf[start_idx].angle_deg;
    uint8_t fin_angle_deg  = scan_buf[end_idx_excl - 1].angle_deg;
    float   init_distance  = scan_buf[start_idx].distance_cm;
    float   fin_distance   = scan_buf[end_idx_excl - 1].distance_cm;

    uint8_t middle_angle_deg = (init_angle_deg + fin_angle_deg) / 2;
    float   avg_dist_cm      = (init_distance + fin_distance) / 2.0f;
    int     angular_width    = (int)fin_angle_deg - (int)init_angle_deg;

    float width_cm = avg_dist_cm * (float)angular_width *
                     (float)M_PI / 180.0f;

    // Servo angle convention: 0 = bot's right (+x), 90 = forward (+y),
    // 180 = bot's left (-x). With +x=right, +y=forward, the servo angle
    // IS the polar angle in body frame measured from +x CCW.
    float alpha   = DEG2RAD((float)middle_angle_deg);
    float dist_mm = avg_dist_cm * 10.0f;

    float lx = SCAN_X_MM + dist_mm * cosf(alpha);
    float ly = SCAN_Y_MM + dist_mm * sinf(alpha);

    float wx, wy;
    body_to_world(lx, ly, &wx, &wy);

    int i;
    for (i = 0; i < obj_count; i++) {
        float dx = wx - objects[i].x;
        float dy = wy - objects[i].y;
        if (dx*dx + dy*dy < DEDUP_RADIUS_MM * DEDUP_RADIUS_MM) {
            return;
        }
    }

    objects[obj_count].x           = wx;
    objects[obj_count].y           = wy;
    objects[obj_count].width_cm    = width_cm;
    objects[obj_count].distance_cm = avg_dist_cm;
    objects[obj_count].id          = obj_count;
    obj_count++;
}

void world_map_finalize_scan(void) {
    int in_obj        = 0;
    int obj_start_idx = 0;

    int i;
    for (i = EDGE_LOOKBACK; i < scan_n; i++) {
        scan_sample_t *cur  = &scan_buf[i];
        scan_sample_t *prev = &scan_buf[i - EDGE_LOOKBACK];

        if (!cur->valid || !prev->valid) {
            if (in_obj) {
                commit_object(obj_start_idx, i);
                in_obj = 0;
            }
            continue;
        }

        float diff = cur->distance_cm - prev->distance_cm;

        if (!in_obj && diff >= EDGE_THRESHOLD_IR) {
            in_obj        = 1;
            obj_start_idx = i;
        } else if (in_obj && diff <= -EDGE_THRESHOLD_IR) {
            commit_object(obj_start_idx, i);
            in_obj = 0;
        }
    }

    scan_n = 0;
}

// ------------------------------------------------------------------
// Boundary logging
// ------------------------------------------------------------------

static void boundary_offsets(boundary_status_t st,
                             float *bx, float *by,
                             boundary_kind_t *kind) {
    switch (st) {
        case BOUNDARY_LEFT:        *bx = CLIFF_L_X_MM;     *by = CLIFF_L_Y_MM;     *kind = KIND_WALL; break;
        case BOUNDARY_FRONT_LEFT:  *bx = CLIFF_FL_X_MM;    *by = CLIFF_FL_Y_MM;    *kind = KIND_WALL; break;
        case BOUNDARY_FRONT_RIGHT: *bx = CLIFF_FR_X_MM;    *by = CLIFF_FR_Y_MM;    *kind = KIND_WALL; break;
        case BOUNDARY_RIGHT:       *bx = CLIFF_R_X_MM;     *by = CLIFF_R_Y_MM;     *kind = KIND_WALL; break;
        case BOUNDARY_FRONT:       *bx = CLIFF_FRONT_X_MM; *by = CLIFF_FRONT_Y_MM; *kind = KIND_WALL; break;
        case HOLE_LEFT:            *bx = CLIFF_L_X_MM;     *by = CLIFF_L_Y_MM;     *kind = KIND_HOLE; break;
        case HOLE_FRONT_LEFT:      *bx = CLIFF_FL_X_MM;    *by = CLIFF_FL_Y_MM;    *kind = KIND_HOLE; break;
        case HOLE_FRONT_RIGHT:     *bx = CLIFF_FR_X_MM;    *by = CLIFF_FR_Y_MM;    *kind = KIND_HOLE; break;
        case HOLE_RIGHT:           *bx = CLIFF_R_X_MM;     *by = CLIFF_R_Y_MM;     *kind = KIND_HOLE; break;
        case HOLE_FRONT:           *bx = CLIFF_FRONT_X_MM; *by = CLIFF_FRONT_Y_MM; *kind = KIND_HOLE; break;
        default:                   *bx = 0; *by = 0; *kind = KIND_WALL; break;
    }
}

void world_map_log_boundary(boundary_status_t st) {
    if (st == BOUNDARY_CLEAR) return;
    if (bnd_count >= MAX_BOUNDARIES) return;

    float bx, by;
    boundary_kind_t kind;
    boundary_offsets(st, &bx, &by, &kind);

    float wx, wy;
    body_to_world(bx, by, &wx, &wy);

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

// ------------------------------------------------------------------
// Output
// ------------------------------------------------------------------

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

void world_map_send_position(void){
    char line[64];

    float theta_deg = RAD2DEG(bot_pose.theta);
    float gui_angle = theta_deg + 90.0f;
    while(gui_angle >= 360.0f) gui_angle -= 360.0f;
    while(gui_angle < 0.0f) gui_angle += 360.0f;

    float x_cm = bot_pose.x / 10.0f;
    float y_cm = bot_pose.y / 10.0f;

    sprintf(line, "P\t%.2f\t%.2f\t%.2f\r\n", x_cm, y_cm, gui_angle);
    uart_sendStr(line);
}
