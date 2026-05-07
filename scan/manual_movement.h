/*
 * manual_movement.h
 *
 *  Created on: Apr 29, 2026
 *      Author: randyc1
 */

#ifndef MANUAL_MOVEMENT_H_
#define MANUAL_MOVEMENT_H_

#include "uart-interrupt.h"
#include "open_interface.h"
#include "movement.h"

typedef enum{

    MOTION_STOP = 0,
    MOTION_FORWARD,
    MOTION_BACKWARD,
    MOTION_LEFT,
    MOTION_RIGHT,
    SCAN

}motion_t;

static void apply_motion(motion_t m, oi_t *sensor_data);

void manual_driving(oi_t *sensor_data);


#endif /* MANUAL_MOVEMENT_H_ */
