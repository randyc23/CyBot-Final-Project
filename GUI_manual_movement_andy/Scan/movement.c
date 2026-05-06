/*
 * movement.c
 *
 *  Created on: Feb 6, 2026
 *      Authors: Andy Knockel, Luc Johnson
 */

#include "open_interface.h"

double move_forward(oi_t *sensor_data, double distance_mm);
double move_backward(oi_t *sensor_data, double distance_mm);
double turn_left(oi_t *sensor, double degrees);
double turn_right(oi_t *sensor, double degrees);
static int get_distance(oi_t *sensor_data);

double move_forward(oi_t *sensor_data, double distance_mm) {
    uint16_t speed;
    uint32_t sum = 0;

    if (distance_mm < 100) {
        speed = 10;
    } else {
        speed = 150;
    }

    oi_setWheels(speed, speed);
    while (sum < distance_mm) {
        sum += get_distance(sensor_data);
    }

    oi_setWheels(0,0);
    return sum;
}

/*
* Function to move the CyBot backwards
* Does not ease in or out like move_forward() and moves much slower for precision
*
* @return: sum, the distance moved as a positive double
*/
double move_backward(oi_t *sensor_data, double distance_mm) {
    uint16_t speed;
    uint32_t sum = 0;

    if (distance_mm < 100) {
        speed = 10;
    } else {
        speed = 100;
    }

    oi_setWheels(-speed, -speed);
    while (sum < distance_mm) {
        //Currently subtracting a negative to add so we can return sum
        sum -= get_distance(sensor_data);
    }

    oi_setWheels(0,0);
    return sum;
}

double turn_left(oi_t *sensor, double degrees) {
    double sum = 0;
    //0.97
    double rotation_corrected = degrees * 0.90;

    oi_setWheels(50, -50);

    while (sum < rotation_corrected) {
        oi_update(sensor);

        sum += sensor -> angle;

    }

    oi_setWheels(0,0);

    return sum;
}

double turn_right(oi_t *sensor, double degrees) {
    double sum = 0;
    //0.97
    double rotation_corrected = degrees * 0.90;

    oi_setWheels(-50, 50);

    while (sum + rotation_corrected >= 0) {
        oi_update(sensor);

        sum += sensor -> angle;

    }

    oi_setWheels(0,0);

    return sum;
}

static int get_distance(oi_t *sensor_data) {
    oi_update(sensor_data);
    return sensor_data -> distance;
}

