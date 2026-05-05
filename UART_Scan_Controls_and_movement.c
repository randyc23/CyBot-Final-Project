/*
 * UART_Scan_Controls_and_movement.c
 *
 *  Created on: Feb 15, 2026
 *      Author: Bot Maxxers
 */



#include <math.h>
#include "cyBot_Scan.h"
#include <stdbool.h>
#include <stdio.h>
#include "movement.h"
#include "uart-interrupt.h"
#include "open_interface.h"


#define BACKUP_MM     150.0
#define PING_STOP_CM   20.0
#define MAX_SCAN_DIST_IR 80.0
#define CYBOT_WIDTH 35.0
#define MAX_IR_SCAN_DIST 70.0

//Global Variable/Arrays
double distances[180];
float currentAngle = 0.0; // in float, right?
int objectNum = 0;

int ini_angle = 0;
double ini_distance = 0.0;

int fin_angle = 0;

cyBOT_Scan_t scan;
char msg[128];
int angleStored[10];
int linear_width[10];
double distance_of_object[10];
int arrayCounter = 0;
double fin_distance = 0.0;
extern volatile int current_speed;
int first_tripped = 0;
int array_pos = 0;

int counter = 0;

int scan_counter = 0;
int objectCoordIndex = 0;
int tooManyObjects = 0; // boolean that is used in a for loop to see if the CyBot is stuck.
//IR sensor global variables

int ir_val[180];
int ir_array_pos = 0;

//IR sensor look-up table

int ir_lookup[36] = {
    2578, 2414, 2270, 2142, 2028, 1925,
    1832, 1748, 1671, 1601, 1536, 1476,
    1421, 1370, 1322, 1278, 1236, 1197,
    1161, 1126, 1094, 1063, 1034, 1007,
    981, 956, 933, 911, 889, 869,
    850, 831, 813, 796, 780, 764
};

float xObjectCoords[20];
float yObjectCoords[20];
float xCliffCoords[20];
float yCliffCoords[20];
int boundariesIndex = 0;
int cliffIndex = 0;
float currentX = 0.0;
float currentY = 0.0;
float validXOpening = 0.0;
float validYOpening = 0.0;
double bestDistance = 0.0;
float bestY = 0.0;
float bestX = 0.0;
int bestI = 0;
int bestJ = 0;
int no = 0;
int manualMode = 0;


/*int main(void) { // some initiliazation code below. 
    oi_t *sensor_data = oi_alloc();
    oi_init(sensor_data);
    uart_init();
    cyBOT_init_Scan(0b0111);
    init_coords();          // initialize coordinate arrays
    IntMasterEnable();

}*/


void init_coords() {
    int r = 0;
    for (r = 0; r < 20; ++r) {
        xObjectCoords[r] = 1000.0;
        yObjectCoords[r] = 1000.0;
        xCliffCoords[r]  = 1000.0;
        yCliffCoords[r]  = 1000.0;
    }
}

// combine movement.c and UART_and Scan-controls.c into one file.

int move_forward(oi_t *sensor_data, double distance_mm) {
    int xClose = 0;
    int yClose = 0;
    // above two variables determine if CyBot is close to an object on x or y position.
    int bumped = 1;
    //Distance measured
    double sum = 0; // distance member in oi_t struct is type double
    //Sets the wheel forward at half the max speed to prevent errors and wheels slipping
    oi_setWheels(current_speed, current_speed);

    while(sum < distance_mm) {
        int xClose = 0;  // reset both of these each iteration.
        int yClose = 0;

         // --- UART interrupt check (set by ISR, not blocking) ---
        // If user pressed spacebar in PuTTY, command_flag was set by UART1_Handler
        // FIXED — only stop on spacebar or 'q':
        if (command_flag && (command_byte == ' ' || command_byte == 'q')) {
            command_flag = 0;
            oi_setWheels(0, 0);
            return -1;
        }
         oi_update(sensor_data);
        // --- Bump sensors ---
        if (sensor_data->bumpLeft || sensor_data->bumpRight) {
            oi_setWheels(0, 0);
            uart_sendStr("Bump! Backing up.\r\n");
            move_backward(sensor_data, -current_speed / 2.0);
            // Use turn_left and turn_right instead from previous labs.
            return 0;
        }

        // --- Cliff sensors ---
        if (sensor_data->cliffLeft || sensor_data->cliffFrontLeft ||
            sensor_data->cliffFrontRight || sensor_data->cliffRight) {
            oi_setWheels(0, 0); // it stops the CyBot. 
            uart_sendStr("Cliff! Backing up.\r\n");
            move_backward(sensor_data, -BACKUP_MM);
            // Update Cliff X/Y Coordinate Arrays. 
        if (cliffIndex < 19) {
            cliffIndex++;
            xCliffCoords[cliffIndex] = (sum / 10.0) * cos(currentAngle * (M_PI / 180.0));
            yCliffCoords[cliffIndex] = (sum / 10.0) * sin(currentAngle * (M_PI / 180.0));
        }
            return 0;
        }
       /* if(sensor_data -> bumpLeft){

            bump_Left(sensor_data);
            sum -= 100;
            oi_setWheels(200, 200);
            bumped = 0;

        }*/

        /*else if(sensor_data -> bumpRight){

            bump_Right(sensor_data);
            sum -= 100;
            oi_setWheels(200, 200);
            bumped = 0;
        }*/
        cyBOT_Scan(90, &scan);
        if (scan.sound_dist > 0 && scan.sound_dist < PING_STOP_CM) {
            oi_setWheels(0, 0);
            sprintf(msg, "Ping: object at %.1f cm - backing up\r\n",
                    scan.sound_dist);
            uart_sendStr(msg);
            move_backward(sensor_data, -BACKUP_MM);
            return 0;
        }
        // loop for the bump and cliff sensors in order to detect whether or not it comes within 15 cm of an object.
        int y;
        int x;
        for (x = 0; x < 20; ++x) { 
            if (fabs(currentX - xCliffCoords[x]) <= 15.0 || fabs(currentX - xObjectCoords[x]) <= 15.0) { // or 150 for 15 centimeters.
                xClose = 1;
            } else {
                continue;
            }
        }
        for (y = 0; y < 20; ++y) {
                if (fabs(currentY - yCliffCoords[y]) <= 15.0 || fabs(currentY - yObjectCoords[y]) <= 15.0) { // or 150 for 15 centimeters.
                yClose = 1;
            } else {
                continue;
            }
        }

        if (xClose == 1 || yClose == 1) {
        oi_setWheels(0, 0);
        // Reset coord tracking so fresh scan doesn't append to stale data
        objectCoordIndex = 0;
        bestDistance = 0.0;
        bestI = -1;
        bestJ = -1;
        // Fresh scan to get current object positions relative to CyBot NOW
        scan_start_stop_send_ir(0, 180, 2, sensor_data);
        // ^^^ This populates xObjectCoords[], yObjectCoords[], objectCoordIndex
        // but it will also call drive_to_smallest at the end — so you need to
        // prevent that. See fix below.
        int decision = drive_through_largest_opening(sensor_data);
        if (decision == -1) {
            // Fewer than 2 objects — path is likely open, just keep going
            uart_sendStr("Few objects detected, continuing forward.\r\n");
            oi_setWheels(current_speed, current_speed); // resume driving
            // do NOT set manualMode
        }
        if (decision == -5) {
            uart_sendStr("Stuck! Botmaxxers, switch to Manual Mode!\r\n");
            manualMode = 1;
            return -1;
        }
}

        //Adds to the sum for the while condition
        sum += sensor_data -> distance;
        

    }
    //stops the wheels
    oi_setWheels(0, 0);
    currentX = currentX + ((sum / 10.0) * cos(currentAngle * ((M_PI / 180))));  // convert mm to cm
    currentY = currentY + ((sum / 10.0) * sin(currentAngle * (M_PI / 180)));
     // two lines above update current position of the CyBot.
    return bumped;

}

void move_backward(oi_t *sensor_data, double distance_mm){

    lcd_init();

    double sum = 0;

    oi_setWheels(-current_speed, -current_speed);

    while(sum > distance_mm){

        oi_update(sensor_data);

        sum += sensor_data -> distance;

        lcd_printf("%.2f", sum);

    }
    currentX = currentX - ((sum / 10.0) * cos(currentAngle * (M_PI / 180)));  // convert mm to cm
    currentY = currentY - ((sum / 10.0) * sin(currentAngle * (M_PI / 180)));
    oi_setWheels(0, 0);

}

void turn_right(oi_t *sensor_data, double degrees) {
    lcd_init();
    //sensor_data -> angle;
    double current = sensor_data -> angle; // angle is in radians.
    double current_degrees = current; // radians into degrees
    double degree_target = current_degrees - degrees; // negative degrees are clockwise.
    oi_setWheels(-100, 100);
    while (fabs(current_degrees) <= fabs(degree_target * 2.5)) {
        timer_waitMillis(125);
        oi_update(sensor_data);
        current += sensor_data -> angle; // angles is stored in radians.
        current_degrees = current; // turns it into degrees. gets moved degrees since last?
        lcd_printf("%.2f", current_degrees);

    }

    
    // Add to turn_right() before oi_setWheels(0,0):
    currentAngle -= degrees;
    while (currentAngle < -180.0f) currentAngle += 360.0f;
        oi_setWheels(0, 0);
    }

void turn_left(oi_t *sensor_data, double degrees) {
    lcd_init();
    double current = sensor_data -> angle;
    double current_degrees = current; // radians into degrees
    double degree_target = (current_degrees + (degrees * 1));
    oi_setWheels(100, -100);
    while (fabs(current_degrees) <= fabs(degree_target * 2.55)) {
        timer_waitMillis(125);
        oi_update(sensor_data);
        // spin right wheel

        current += sensor_data -> angle;
        current_degrees = current;
        lcd_printf("%.2f", current_degrees);
     }

     // Add to turn_left() before oi_setWheels(0,0):
    currentAngle += degrees;
    while (currentAngle > 180.0f) currentAngle -= 360.0f;
    // At the end of turn_left(), after the currentAngle update:
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



int drive_to_smallest( oi_t *sensor_data){
          int bumped = 0;

           int i = 0;
           int minR = 0;
          // int minA = 0;
           int angle_of_minR = 0;
           minR = linear_width[0];
           angle_of_minR = angleStored[0];
           double distance = distance_of_object[0];

           if(scan_counter >= 2){

               for(i = 0; i < 10; ++i){

                   if(linear_width[i] == 0){
                       continue;
                   }

                   if (linear_width[i] > minR) {
                       minR = linear_width[i];
                       angle_of_minR = angleStored[i];
                       distance = distance_of_object[i];
                   }

               }

           }
           else{

           for (i = 0; i < 10; ++i) {

               if(linear_width[i] == 0){
                   continue;
               }

               if (linear_width[i] < minR) {
                   minR = linear_width[i];
                   angle_of_minR = angleStored[i];
                   distance = distance_of_object[i];
               }

           }
           }
           if(angle_of_minR == 90){

               bumped = move_forward(sensor_data, (distance * 5) - 100);

           }

           else if(angle_of_minR > 90){

               turn_left(sensor_data, (angle_of_minR - 90));

               bumped = move_forward(sensor_data, (distance * 5) - 100);

           }

           else if(angle_of_minR < 90){

               turn_right(sensor_data, (90 - angle_of_minR));

               bumped = move_forward(sensor_data, (distance * 5) - 100);

           }



           return bumped;

}

void send_angle_and_dist(int angle, double dist) { //Sends the data to PuTTY
   char st[200];

   sprintf(st, " Angle: %i         Distance: %.02lf\r\n", angle, dist);
   // for each character in st, send Byte.

   int i = 0;

   for(i = 0; i < strlen(st); i++) {

       uart_sendChar(st[i]);

   }
}

void send_angle_and_dist_object(int obj_num, int angle, double distance, int width){

    char st[200];

    sprintf(st, "Object %i detected at distance %.2lf, angle %i, width %i\r\n", obj_num, distance, angle, width);

    int i = 0;

       for(i = 0; i < strlen(st); i++) {

           uart_sendChar(st[i]);

       }

}

void send_angle_and_dist_and_ir(int angle, float dist, int ir_value, float ir_value_converted){

    char st[200];

    sprintf(st, "Angle: %i      Distance: %.2lf     IR: %i  IR Converted:%.2lf\r\n", angle, dist, ir_value, ir_value_converted);

    int i = 0;

    for(i = 0; i < strlen(st); i++){

        uart_sendChar(st[i]);

    }

}

void scan_start_stop_send(int start, int stop, int angle_step, oi_t *sensor_data){ //Scans from start to stop stopping at how many angle_step and then sends it to PuTTY

    cyBOT_Scan_t scan;

    if(stop < start){

        return;

    }

    int i = 0;
    double distance = 0;

    for(i = start; i <= stop; i+= angle_step){

            cyBOT_Scan(i, &scan);

            distance = scan.sound_dist;

            array_pos++;

            detect_object(i, distance,array_pos);



            //send_angle_and_dist(i, distance);


        }

    drive_to_smallest(sensor_data);

}

int scan_start_stop_send_ir(int start, int stop, int angle_step, oi_t *sensor_data){

    scan_counter++;

    cyBOT_Scan_t scan;
    objectCoordIndex = 0;
    array_pos = 0;
    arrayCounter = 0;
    objectNum = 0;
    first_tripped = 0;
    int i = 0;

    for (i = 0; i < 10; i++) {
        linear_width[i] = 0;
        angleStored[i] = 0;
        distance_of_object[i] = 0;
    }

    int average_ir[4];

    i = 0;
    int x = 0;
    int ir_value_print = 0;
    int distance = 0;
    int ir_distance = 0;

    for(i = start; i <= stop; i+= angle_step){
        for(x = 0; x < 4; x++){

            cyBOT_Scan(i, &scan);

            average_ir[x] = scan.IR_raw_val;

        }

        cyBOT_Scan(i, &scan); // for testing purposes

        distance  = scan.sound_dist; // for testing purposes

        ir_value_print = (average_ir[0] + average_ir[1] + average_ir[2] + average_ir[3]) / 4; // Get average of measure IR values
        // You can use 3 values. 2 values is too little.
        int f = 0;

        int difference = 8000;

        for(f = 0; f < 36; f++){ //iterate through lookup table finding the least difference


            if(ir_value_print <= 764){ //if IR value is below 764 it is out of the IR range of 80cm

                ir_distance = 80;

                continue;

            }

            else if(ir_value_print >= 2578){ //if IR value is above 2578 it is super close

                ir_distance = 1;


            }

            int system_difference = ir_value_print - ir_lookup[f]; //Get the difference from the ir table

            if(system_difference < difference && !(system_difference < 0)){ //Check to see if the difference is less than the last calculated and that the value is also not negative

                difference = system_difference;

                ir_distance = 10 + (2 * f); //Math for converting IR value to cm

            }

        }


        //send_angle_and_dist_and_ir(i, distance, ir_value_print, ir_distance);

        detect_object(i, ir_distance, array_pos);

        array_pos++;

    }

    /*int returned = drive_to_smallest(sensor_data);

    if(returned == 1){

        return 1;

    }*/

    return 0;

}




void detect_object(int angle, double distance, int array_pos){

    double difference = 0;
    double average_distance = 0.0;
    int middle_angle = 0;
    int angular_width = 0;
    int linear_width_in = 0.0;


    distances[array_pos] = distance;

    if(array_pos > 2){

        if(array_pos > 4){
/*
    if(array_pos == 5){
        difference = distances[array_pos] - distances[array_pos - 1];
    }
    else if(array_pos == 6){
            difference = distances[array_pos] - distances[array_pos - 2];
    }
    else if(array_pos == 7){
            difference = distances[array_pos] - distances[array_pos - 3];
    }
    else if(array_pos == 8){
            difference = distances[array_pos] - distances[array_pos - 4];
    }
    else if(array_pos == 9){
            difference = distances[array_pos] - distances[array_pos - 5];
    }
    else{
        difference = distances[array_pos] - distances[array_pos - 5];
    }

*/
            if(array_pos < 3){

                difference = 3;

            }

            else{

               // difference = distances[array_pos] - ((distances[array_pos] + distances[array_pos - 1] + distances[array_pos - 2]) / 3);
                difference = distances[array_pos] - distances[array_pos - 5];
            }


            if(difference <= -6 ){


                ini_distance = distance;
                first_tripped = 1;
                ini_angle = angle;


            }

            else if((difference > 6) && first_tripped == 1){ // executes when stop detecting object

                fin_angle = angle -  2;
                fin_distance = distance;
                average_distance = (ini_distance + distances[array_pos - 1])/2;
                middle_angle = (ini_angle + fin_angle)/2;
                angular_width = (fin_angle - ini_angle);
                linear_width_in = average_distance * ((angular_width * M_PI) / 180);
                objectNum++;
                first_tripped = 0;
                counter = 0;
                linear_width[arrayCounter] = linear_width_in;
                angleStored[arrayCounter] = middle_angle;
                distance_of_object[arrayCounter] = average_distance;


               if (objectCoordIndex < 20) {
                    xObjectCoords[objectCoordIndex] = fin_distance * (cos(fin_angle * (M_PI / 180))); // uses geometry in order to do it.
		            yObjectCoords[objectCoordIndex] = fin_distance * (sin(fin_angle * (M_PI / 180)));
                    objectCoordIndex++;
                } else {
                    // do nothing
                }
                arrayCounter += 1;
                send_angle_and_dist_object(objectNum, middle_angle, average_distance, angular_width);
            }
        }
    }
}

int drive_through_largest_opening(oi_t *sensor_data) {
    // Reset function variables
    bestDistance = 0.0;
    bestI = -1;
    bestJ = -1;
                // detect if too many objects are true. The first obj coordinates will belong to xObjectCoords[0] and 
                // yobjectCoords[0], respectively.
        if (objectCoordIndex < 2) {
            uart_sendStr("Not enough objects detected to find an opening.\r\n");
            return -1;
        }     
                int i = 0;
                int j = 0;
                bestI = -1;
                bestJ = -1;
                for (i = 0; i < objectCoordIndex; ++i) {
                    double dx = xObjectCoords[i] - currentX;
                    double dy = yObjectCoords[i] - currentY;
                    double dist_to_obj = sqrt(dx * dx + dy * dy);
                    if (dist_to_obj > MAX_SCAN_DIST_IR) { // if dist > 80.0 cm.
                        continue;
                    }
                    for (j = i + 1; j < objectCoordIndex; ++j) {
                        double dx2 = xObjectCoords[j] - currentX;
                        double dy2 = yObjectCoords[j] - currentY;
                        double dist_to_obj_j = sqrt(dx2 * dx2 + dy2 * dy2);

                        if (dist_to_obj_j > MAX_IR_SCAN_DIST) {
                            continue;
                        
                        }
                        double x = xObjectCoords[i] - xObjectCoords[j];
                        double y = yObjectCoords[i] - yObjectCoords[j];
                        double distance = sqrt(x * x + y * y);
                        if (distance <= CYBOT_WIDTH) {
                            continue;
                        }
                        // need to record the biggest opening to go through.
                        if (distance > bestDistance) {
                            bestI = i;
                            bestJ = j;
                            bestDistance = distance;
                            bestX = (xObjectCoords[i] + xObjectCoords[j]) / 2.0f;
                            bestY = (yObjectCoords[i] + yObjectCoords[j]) / 2.0f;
                        }
                        
                    }
                }
        if (bestI == -1) {
            uart_sendStr("No passable gap found between objects.\r\n");
            tooManyObjects = 1; // true
            return -5;
        }
        tooManyObjects = 0;
    // --- Step 1: Find the largest valid gap between any two object coords ---
    // --- Step 2: Compute angle to the opening midpoint ---
    // atan2f gives angle in radians relative to +X axis; convert to degrees
    float angle_to_opening = atan2f(bestY, bestX) * (180.0f / M_PI);
    sprintf(msg, "Best opening midpoint: (%.1f, %.1f), gap: %.1f cm, heading: %.1f deg\r\n",
            bestX, bestY, bestDistance, angle_to_opening);
    uart_sendStr(msg);
    // --- Step 3: Compute how much to turn from current heading ---
    // currentAngle must be maintained in degrees in your main loop
    float turn_needed = angle_to_opening - currentAngle;
    // Normalize to [-180, 180] so we always take the shorter turn
    while (turn_needed > 180.0f)  turn_needed -= 360.0f;
    while (turn_needed < -180.0f) turn_needed += 360.0f;
    sprintf(msg, "Turning %.1f degrees to face opening.\r\n", turn_needed);
    uart_sendStr(msg);
    // --- Step 4: Execute the turn ---
    if (turn_needed > 1.0f) {                   // small deadband to avoid micro-turns
        turn_left(sensor_data, turn_needed);
    } else if (turn_needed < -1.0f) {
        turn_right(sensor_data, -turn_needed);   // turn_right takes a positive value
    }
    // Update heading after turning
    currentAngle += turn_needed;
    // Normalize currentAngle too
    while (currentAngle > 180.0f)  currentAngle -= 360.0f;
    while (currentAngle < -180.0f) currentAngle += 360.0f;
    // --- Step 5: Drive to the midpoint of the opening ---
    // Distance to the midpoint from current position (CyBot is at origin of its local frame)
    double dist_to_opening_mm = sqrt(bestX * bestX + bestY * bestY) * 10.0;  // converts to millimeters.
    // multiply by 10 if your coords are in cm and move_forward takes mm
    sprintf(msg, "Driving %.1f mm toward opening.\r\n", dist_to_opening_mm);
    uart_sendStr(msg);
    int result = move_forward(sensor_data, dist_to_opening_mm);
    if (result == 0) {
        uart_sendStr("Bumped or cliffed while approaching opening.\r\n");
    } else if (result == -1) {
        uart_sendStr("User stopped movement.\r\n");
    } else {
        uart_sendStr("Successfully passed through opening!\r\n");
    }
    return result;
}
