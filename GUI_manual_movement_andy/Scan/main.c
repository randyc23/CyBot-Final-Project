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
    scan_init();
}

int main(void) {
    init_everything();
    /*
    right_calibration = 8300;
    left_calibration = 35700;

    scan_read scan_vals;

    while (1) {
        scan_feature_enable(2);
        scan(&scan_vals, 90, 1);

        lcd_printf("%.2lf", scan_vals.ping);
        if (received_char == ' ') {
            scan_feature_enable(3);
            scan_range(0, 180, 2, 3);
        }
        received_char = '\0';
    }*/

    scan_feature_enable(3);

    right_calibration = 8600;
    left_calibration = 36300;

    world_map_init();

    while(1) {

       oi_update(sensor_data);
       pose_update(sensor_data);

       update_cliff_sensors(sensor_data);
       /*boundary_status_t s = avoid_hole_boundary(sensor_data);
       if (s != BOUNDARY_CLEAR){
           continue;
       }*/

       manual_driving(sensor_data);
       if (received_char == 'x') {
           world_map_print_pose();
           received_char = '0';
       }

    }

}
