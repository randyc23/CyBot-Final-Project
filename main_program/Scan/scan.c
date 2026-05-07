/*
 * scan.c
 *
 *  Created on: Apr 26, 2026
 *  Author: Andy Knockel
 */
#include "scan.h"

uint8_t enable_config;

/*
 * Initializes all the necessary components for the scan functions
 * Make sure to call the servo_calibrate() function after this
 */
void scan_init(void) {
    uart_interrupt_init();
    button_init();
    adc_init();
    ping_init();
    servo_init();
}

/*
 * Sets whether scan will use IR, Ping, or both
 * @param config set 0th bit for IR, 1st for Ping. More simply: 0 = neither, 1 = just IR, 2 = just Ping, 3 = both
 */
void scan_feature_enable(uint8_t config) {
    enable_config = config;
}

/*
 * Scans over a range of angles, for a singular scan, see scan(). IR and Ping are enabled/disabled by scan_feature_enable()
 * Data is automatically sent to home base
 *
 * @param start_angle angle the scan should start at relative to the cybot, 0 is all the way to the right
 * @param end_angle angle the scan should end at relative to the cybot, 180 is all the way left
 * @param increment how many degrees the scanner moves per tick. if this value doesn't divide the difference between start and end angle, it may miss a few degrees at the end
 * @param num_scans the number of times the Ping or IR value is read per angle tick
 */
void scan_range(uint8_t start_angle, uint8_t end_angle, uint8_t increment, uint8_t num_scans) {
    if (start_angle > end_angle) return; //the start angle can't be further than the end angle

    scan_read scan_vals;

    char message[32] = "";

    strcpy(message, "SS\r\n");
    uart_sendStr(message);

    uint8_t i;
    for (i = start_angle; i <= end_angle; i += increment) {
        scan(&scan_vals, i, num_scans);
        sprintf(message, "S\t%d\t%d\t%.2lf\r\n", 90 - i, scan_vals.ir, scan_vals.ping);
        uart_sendStr(message);
    }

    strcpy(message, "SE\r\n");
    uart_sendStr(message);
}

/*
 * Scans a single angle num_scans times and averages the value
 * @param scan a pointer to a scan_read object, gets updated by the function
 * @param angle angle the scan will happen at
 * @param num_scans the number of times the Ping or IR value is read before being averaged
 */
void scan(scan_read *scan_vals, uint8_t angle, uint8_t num_scans) {
    uint32_t ir_total = 0;
    float ping_total = 0;

    servo_move(angle);

    uint8_t i;
    for (i = 0; i < num_scans; i++) {
        if (enable_config & 0b01) {
            ir_total += adc_read();
        }
        if (enable_config & 0b10) {
            ping_total += ping_getDistance();
        }
    }
    //if Ping or IR is disabled, their values should be 0 and -1 respectively
    scan_vals->ir = (ir_total != 0) ? ir_total / num_scans : 0;
    scan_vals->ping = (ping_total != 0) ? ping_total / num_scans : 0;
}
