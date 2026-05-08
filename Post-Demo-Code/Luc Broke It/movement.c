/*
 * movement.c
 *
 *  Created on: Feb 5, 2026
 *      Author: randyc1
 */

#include "open_interface.h"
#include "world_map.h"
#include "music.h"

int move_forward(oi_t *sensor_data, double distance_mm) {

    int bumped = 1;
    //Distance measured
    double sum = 0; // distance member in oi_t struct is type double
    //Sets the wheel forward at half the max speed to prevent errors and wheels slipping
    oi_setWheels(100, 100);

    while(sum < distance_mm) {
        oi_update(sensor_data);
        pose_update(sensor_data);

        if(sensor_data -> bumpLeft){

            bump_Left(sensor_data);
            sum -= 100;
            oi_setWheels(200, 200);
            bumped = 0;

        }

        else if(sensor_data -> bumpRight){

            bump_Right(sensor_data);
            sum -= 100;
            oi_setWheels(200, 200);
            bumped = 0;
        }

        //Adds to the sum for the while condition
        sum += sensor_data -> distance;
    }
    //stops the wheels
    oi_setWheels(0, 0);

    return bumped;

}

void move_backward(oi_t *sensor_data, double distance_mm){

    lcd_init();

    double sum = 0;

    oi_setWheels(-100, -100);

    while(sum > distance_mm){

        oi_update(sensor_data);
        pose_update(sensor_data);

        sum += sensor_data -> distance;

    }

    oi_setWheels(0, 0);

}

void turn_right(oi_t *sensor_data, double degrees) {
    lcd_init();
    //sensor_data -> angle;
    double current = sensor_data -> angle; // angle is in radians.
    double current_degrees = current / .324056; // radians into degrees
    double degree_target = current_degrees - degrees; // negative degrees are clockwise.
    oi_setWheels(-100, 100);
    while (abs(current_degrees) <= abs(degree_target * 2.5)) {
        timer_waitMillis(125);
        oi_update(sensor_data);
        pose_update(sensor_data);

        current += sensor_data -> angle; // angles is stored in radians.
        current_degrees = current / .324056; // turns it into degrees. gets moved degrees since last?


    }
    oi_setWheels(0, 0);


}

void turn_left(oi_t *sensor_data, double degrees) {
    lcd_init();
    double current = sensor_data -> angle;
    double current_degrees = current / .324056; // radians into degrees
    double degree_target = (current_degrees + (degrees * 1));
    oi_setWheels(100, -100);
    while (abs(current_degrees) <= abs(degree_target * 2.55)) {
        timer_waitMillis(125);
        oi_update(sensor_data);
        // spin right wheel
        pose_update(sensor_data);

        current += sensor_data -> angle;
        current_degrees = current / .324056;
     }
    oi_setWheels(0, 0);
}

void bump_Left(oi_t *sensor_data){

    move_backward(sensor_data, -90);
    turn_right(sensor_data, 90);
    move_forward(sensor_data, 250);
    turn_left(sensor_data, 90);


}

void bump_Right(oi_t *sensor_data){

    move_backward(sensor_data, -90);
    turn_left(sensor_data, 90);
    move_forward(sensor_data, 250);
    turn_right(sensor_data, 90);

}



