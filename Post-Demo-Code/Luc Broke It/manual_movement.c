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
#include "scan.h"
#include "world_map.h"
#include "movement.h"
#include "music.h"
#include "lcd_face.h"


#define KEY_TIMEOUT_MS 200

static motion_t current_motion = MOTION_STOP;
static int last_key_ms = 0;

char input = '0';

static void apply_motion(motion_t m, oi_t *sensor_data){

    switch(m){

        case MOTION_FORWARD: oi_setWheels(150, 150); break;
        case MOTION_BACKWARD: oi_setWheels(-150, -150); break;
        case MOTION_LEFT: oi_setWheels(70, -70); break;
        case MOTION_RIGHT: oi_setWheels(-70, 70); break;
        case SCAN:

            scan_range(0, 180, 1, 3);

            break;
        case MOTION_STOP:
        default: oi_setWheels(0, 0); break;

    }
    current_motion = m;

}

void manual_driving(oi_t *sensor_data){



    if(sensor_data -> bumpLeft){
        oi_setWheels(0,0);
        load_songs(3);
        uart_sendStr("B\tLeft\r\n");
        move_backward(sensor_data, -90);
        apply_motion(MOTION_STOP, sensor_data);
        return;
    }

    if(sensor_data -> bumpRight){
        oi_setWheels(0,0);
        load_songs(3);
        uart_sendStr("B\tRight\r\n");
        move_backward(sensor_data, -90);
        apply_motion(MOTION_STOP, sensor_data);
        return;
    }

    if(return_flag()){

        reset_flag();
        input = return_character();

        if(input == 'p' || input == 'P'){  //World map print
            world_map_print();
            last_key_ms = timer_getMillis();
            return;
        }

        if(input == 'r' || input == 'R'){ //World map reset
            world_map_reset();
            uart_sendStr("[MAP] reset\r\n");
            last_key_ms = timer_getMillis();
            return;
        }

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

        if(requested != current_motion){
            apply_motion(requested, sensor_data);
        }

        last_key_ms = timer_getMillis();

    }

    if(current_motion != MOTION_STOP && (timer_getMillis() - last_key_ms) > KEY_TIMEOUT_MS){
        apply_motion(MOTION_STOP, sensor_data);
    }
}


