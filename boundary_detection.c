/*
 *
 * Boundry Detection using the cliff sensors
 *
 * On the white tape(boundary) the IR sensors will return a high value, usually above 2600
 * On the black tape(hole) the IR sensors will return a low value, usually below 200-300
 * On a regular surface the IR sensor is between the values of 500-2400
 *
 */

#include "open_interface.h"
#include "uart-interrupt.h"
#include "lcd.h"
#include "movement.h"
#include "boundary_detection.h"

int cliffLeft = 0;
int cliffFrontLeft = 0;
int cliffFrontRight = 0;
int cliffRight = 0;

void update_cliff_sensors(oi_t *sensor_data){ //Updates the cliff sensors

    oi_update(sensor_data);

    cliffLeft = sensor_data -> cliffLeftSignal;
    cliffFrontLeft = sensor_data -> cliffFrontLeftSignal;
    cliffFrontRight = sensor_data -> cliffFrontRightSignal;
    cliffRight = sensor_data -> cliffRightSignal;

}

void send_cliff_uart(int cliffLeft, int cliffFrontLeft, int cliffFrontRight, int cliffRight){ //Use for testing purposes

    char st[200];

    sprintf(st, "left: %d\nfront left: %d\nfront right: %d\nright: %d", cliffLeft, cliffFrontLeft, cliffFrontRight, cliffRight);

    int i = 0;

    for(i = 0; i < strlen(st); i++){

        uart_sendChar(st[i]);

    }

}

void send_cliff_lcd(int cliffLeft, int cliffFrontLeft, int cliffFrontRight, int cliffRight){ //Use for testing purposes

    lcd_printf("left: %d\nfront left: %d\nfront right: %d\nright: %d", cliffLeft, cliffFrontLeft, cliffFrontRight, cliffRight);

    timer_waitMillis(100);

}

boundary_status_t avoid_hole_boundary(oi_t *sensor_data){

    //Boundary detection thresholds

    int l_bound = cliffLeft > 2600;
    int fl_bound = cliffFrontLeft > 2600;
    int fr_bound = cliffFrontRight > 2600;
    int r_bound = cliffRight > 2600;

    //Hole detection thresholds

    int l_hole = cliffLeft < 250;
    int fl_hole = cliffFrontLeft < 250;
    int fr_hole = cliffFrontRight < 250;
    int r_hole = cliffRight < 250;

    //Determine if any of the sensors were set off

    int any_bound = l_bound || fl_bound || fr_bound || r_bound;
    int any_hole = l_hole || fl_hole || fr_hole || r_hole;

    if(!any_bound && !any_hole){
        return BOUNDARY_CLEAR;
    }

    //Stop movement

    oi_setWheels(0, 0);

    boundary_status_t status;

    if(any_bound){ //Boundary detection

        if(fl_bound && fr_bound){status = BOUNDARY_FRONT;}
        else if(fl_bound) {status = BOUNDARY_FRONT_LEFT;}
        else if(fr_bound) {status = BOUNDARY_FRONT_RIGHT;}
        else if(l_bound) {status = BOUNDARY_LEFT;}
        else {status = BOUNDARY_RIGHT;}

    } else { //Cliff detection

        if(fl_hole && fr_hole) {status = HOLE_FRONT;}
        else if(fl_hole) {status = HOLE_FRONT_LEFT;}
        else if(fr_hole) {status = HOLE_FRONT_RIGHT;}
        else if(l_bound) {status = HOLE_LEFT;}
        else {status = HOLE_RIGHT;}

    }

    //Back away from the hazard

    oi_setWheels(-100, -100);
    timer_waitMillis(1000);
    oi_setWheels(0, 0);

    //Turn away based on which side trip
    switch (status){

        case BOUNDARY_LEFT: case BOUNDARY_FRONT_LEFT:
        case HOLE_LEFT:     case HOLE_FRONT_LEFT:
            //Hazard is on the left -> turn right
            turn_right(sensor_data, 90);
            break;

        case BOUNDARY_RIGHT: case BOUNDARY_FRONT_RIGHT:
        case HOLE_RIGHT:     case HOLE_FRONT_RIGHT:
            //Hazard on right -> turn left;
            turn_left(sensor_data, 90);
            break;

        case BOUNDARY_FRONT: case HOLE_FRONT:
        default:
            turn_right(sensor_data, 180);
            break;

    }

    oi_setWheels(0, 0);
    return status;

}

