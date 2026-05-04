

/**
 * main.c
 */

#include "open_interface.h"
#include "uart-interrupt.h"
#include "boundary_detection.h"
#include "lcd.h"
#include "Timer.h"
#include "movement.h"
#include "manual_movement.h"
#include "scan.h"
#include "servo.h"
#include "world_map.h"

int main(void)
{
       timer_init(); // Must be called before lcd_init(), which uses timer functions
       lcd_init();
       scan_init();

       scan_feature_enable(3);

       oi_t *sensor_data = oi_alloc(); // do this only once at start of main()
       oi_init(sensor_data); // do this only once at start of main()

       //servo_calibrate();

       right_calibration = 8500; //Cybot 23 in overflow
       left_calibration = 35700; //cybot 23 in overflow

       world_map_init();

       while(1){

           oi_update(sensor_data);
           pose_update(sensor_data);

           update_cliff_sensors(sensor_data);
           boundary_status_t s = avoid_hole_boundary(sensor_data);
           if (s != BOUNDARY_CLEAR){
               continue;
           }

           manual_driving(sensor_data);

       }

}
