/*
 * movement.h
 *
 *  Created on: Feb 5, 2026
 *      Author: randyc1
 */

#ifndef MOVEMENT_H_
#define MOVEMENT_H_

#include "open_interface.h"

int move_forward(oi_t *sensor_data, double distance_mm);

void turn_right(oi_t *sensor_data, double degrees);

void turn_left(oi_t *sensor_data, double degrees);

void move_backward(oi_t *sensor_data, double distance_mm);

void bump_Left(oi_t *sensor_data);

void bump_Right(oi_t *sensor_data);

#endif /* MOVEMENT_H_ */
