/*
 * manual_movement.c
 *
 *  Created on: Apr 29, 2026
 *      Author: randyc1
 */

#include "uart-interrupt.h"
#include "open_interface.h"
#include "manual_movement.h"
#include "Timer.h"
#include "UART_and_Scan_controls.h"
#include "cyBot_Scan.h"

#define KEY_TIMEOUT_MS 150

static motion_t current_motion = MOTION_STOP;
static int last_key_ms = 0;

char input = '0';

static int apply_motion(motion_t m, oi_t *sensor_data){

    int returned = 0;

    switch(m){

        case MOTION_FORWARD: oi_setWheels(150, 150); break;
        case MOTION_BACKWARD: oi_setWheels(-150, -150); break;
        case MOTION_LEFT: oi_setWheels(100, -100); break;
        case MOTION_RIGHT: oi_setWheels(-100, 100); break;
        case SCAN:

            while(returned == 0){

            returned = scan_start_stop_send_ir(0, 180, 1, sensor_data);


            }
            break;
        case MOTION_STOP:
        default: oi_setWheels(0, 0); break;

    }
    current_motion = m;

}

void manual_driving(oi_t *sensor_data){

    if(return_flag()){

        reset_flag();
        input = return_character();

        motion_t requested = current_motion;
        switch(input){

                    case 'w': case 'W':
                        requested = MOTION_FORWARD;
                        break;

                    case 's': case 'S':
                        requested = MOTION_BACKWARD;
                        break;

                    case 'a': case 'A':
                        requested = MOTION_LEFT;
                        break;

                    case 'd': case 'D':
                        requested = MOTION_RIGHT;
                        break;

                    case 'e': case 'E':
                        requested = SCAN;
                        break;

                    case ' ':
                        requested = MOTION_STOP;
                        break;
                    //Can add more cases if needed

                    default:
                        //Nothing
                        break;

         }




         if (sensor_data->bumpLeft) {
             oi_setWheels(0, 0);
             uart_sendStr("B Left\r\n");
             move_backward(sensor_data, -50);
         }
        if (sensor_data->bumpRight) {
             oi_setWheels(0, 0);
             uart_sendStr("B Right\r\n");
             move_backward(sensor_data, -50);
         }
        if (sensor_data->cliffLeft) {
             oi_setWheels(0, 0);
             uart_sendStr("C Left\r\n");
             move_backward(sensor_data, -50);
         }
         if (sensor_data->cliffRight) {
             oi_setWheels(0, 0);
             uart_sendStr("C Right\r\n");
             move_backward(sensor_data, -50);
         }
         if (sensor_data->cliffFrontLeft) {
             oi_setWheels(0, 0);
             uart_sendStr("C Front Left\r\n");
             move_backward(sensor_data, -50);
         }
         if (sensor_data->cliffFrontRight) {
             oi_setWheels(0, 0);
             uart_sendStr("C Front Right\r\n");
             move_backward(sensor_data, -50);
         }        
        if(requested != current_motion){
            apply_motion(requested, sensor_data);
        }

        last_key_ms = timer_getMillis();

    }

    if(current_motion != MOTION_STOP && (timer_getMillis() - last_key_ms) > KEY_TIMEOUT_MS){
        apply_motion(MOTION_STOP, sensor_data);
    }
}


