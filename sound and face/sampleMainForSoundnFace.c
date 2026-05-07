#include "uart-interrupt.h"
#include "cyBot_Scan.h"
#include "open_interface.h"
#include "lcd.h"
#include "Timer.h"
#include "movement.h"
#include <math.h>
#include <stdint.h>
#include "cyBot_uart.h"
#include "uart.h"
#include "music.h"
#include "uart-interrupt.h"
#include "lcd_face.h"




int main(void) {
    oi_t *sensor_data = oi_alloc();
    oi_init(sensor_data);
    uart_interrupt_init();
    lcd_init();
    face_init();

    while (1) {
      load_songs(1);
      oi_update(sensor_data);
      face_run();
    }
    oi_free(sensor_data);

}
