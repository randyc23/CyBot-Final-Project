/*
 * boundary_detection.h
 *
 *  Created on: Apr 28, 2026
 *      Author: randyc1
 */

#ifndef BOUNDARY_DETECTION_H_
#define BOUNDARY_DETECTION_H_

#include "open_interface.h"
#include "uart-interrupt.h"
#include "lcd.h"
#include "movement.h"

typedef enum{

    BOUNDARY_CLEAR = 0,
    BOUNDARY_LEFT,
    BOUNDARY_FRONT_LEFT,
    BOUNDARY_FRONT_RIGHT,
    BOUNDARY_RIGHT,
    BOUNDARY_FRONT,
    HOLE_LEFT,
    HOLE_FRONT_LEFT,
    HOLE_FRONT_RIGHT,
    HOLE_RIGHT,
    HOLE_FRONT

}boundary_status_t;

void update_cliff_sensors(oi_t *sensor_data);

void send_cliff_uart(int cliffLeft, int cliffFrontLeft, int cliffFrontRight, int cliffRight);

void send_cliff_lcd(int cliffLeft, int cliffFrontLeft, int cliffFrontRight, int cliffRight);

boundary_status_t avoid_hole_boundary(oi_t *sensor_data);

#endif /* BOUNDARY_DETECTION_H_ */
