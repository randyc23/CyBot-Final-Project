#include "Timer.h"
#include "lcd.h"
#include "open_interface.h"
#include "boundary_detection.h"
#include "manual_movement.h"
#include "world_map.h"
#include "movement.h"
#include "scan.h"

static oi_t *sensor_data;

void init_everything(void) {
    timer_init();
    lcd_init();
    sensor_data = oi_alloc();
    oi_init(sensor_data);
    uart_interrupt_init();

    right_calibration = 7000;
    left_calibration = 35100;
    scan_init();
}

void do_manual_drive(void) {
    scan_feature_enable(3);

   oi_update(sensor_data);
   pose_update(sensor_data);

   uint32_t last_pose_send_ms = 0;
   uint32_t now = timer_getMillis();
   if (now - last_pose_send_ms > 100) {
       world_map_send_position();
       last_pose_send_ms = now;
   }

   update_cliff_sensors(sensor_data);
   /*boundary_status_t s = avoid_hole_boundary(sensor_data);
   if (s != BOUNDARY_CLEAR){
       continue;
   }*/

   manual_driving(sensor_data);
}

/*void do_scan_stuff(void) {
    //scan_read scan_vals;

    //scan_feature_enable(2);
    //scan(&scan_vals, 90, 1);

    //lcd_printf("%.2lf", scan_vals.ping);

    if (received_char == ' ') {
        scan_feature_enable(3);
        scan_range(0, 180, 1, 3);
    }
    received_char = '\0';
}*/



int main(void) {
    init_everything();

    world_map_init();

    while (1) {
        do_manual_drive();
        //do_scan_stuff();
    }

}








