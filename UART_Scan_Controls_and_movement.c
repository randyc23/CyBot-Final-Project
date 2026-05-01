/*
 * UART_Scan_Controls_and_movement.c
 *
 *  Created on: Feb 15, 2026
 *      Author: BotMaxxers
 */
#include <math.h>
#include "cyBot_Scan.h"
#include <stdbool.h>
#include <stdio.h>
#include "movement.h"
#include "uart-interrupt.h"
#include "open_interface.h"



/*
  This combines a UART and a movement file from Lab 2 and lab 7. Those functions combined together
  are placed inside of here. My main ideas is to use keys on a keyboard to move the CyBot (wasd), quit (q), and
  to switch to manual mode (m?). If the CyBot is stuck, it will print out a message (PuTTy or GUI) telling us the 
  CyBot is stuck, and we should switch to manual mode. The switiching from autonomous to manual is always done by us, 
  and not automatically by the programs. 
  
  It will use the Cliff Sensors and the Bump Sensors to detect if the CyBot is close to another object found or not found.
  

  
*/


//Global Variable/Arrays


double distances[180];
int objectNum = 0;

int ini_angle = 0;
double ini_distance = 0.0;

int fin_angle = 0;

int angleStored[10];
int linear_width[10];
double distance_of_object[10];
int arrayCounter = 0;
double fin_distance = 0.0;

int first_tripped = 0;
int array_pos = 0;

int counter = 0;

int scan_counter = 0;
int objectCoordIndex = 0;

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

float[] xCoords = new float[1000];
float[] yCoords = new float[1000];
int boundariesIndex = 0;

if (dist_cm < SAFE_DISTANCE_CM) { // says if
        char msg[64];
        sprintf(msg, "Object at angle %d: %.1f cm - too close!\r\n",
        angles[i], dist_cm);
        uart_sendStr(msg);
        return 1;
}

int cliffIndex = 0;
float[] xObjectCoords = new float[10];
float[] yObjectCoords = new float[10];
float[] xCliffCoords = new float[20];
float[] yCliffCoords = new float[20];
float currentX = 0.0;
float currentY = 0.0;

// Get Methods That return values.
public float getCurrentX() {
    return currentX;
}
public float getCurrentY() {
    return currentY;
}

// combine movement.c and UART_and Scan-controls.c into one file.


int move_forward(oi_t *sensor_data, double distance_mm) {
    int bumped = 1;
    //Distance measured
    double sum = 0; // distance member in oi_t struct is type double
    //Sets the wheel forward at half the max speed to prevent errors and wheels slipping
    oi_setWheels(200, 200);

    while(sum < distance_mm) {
       

         // --- UART interrupt check (set by ISR, not blocking) ---
        // If user pressed spacebar in PuTTY, command_flag was set by UART1_Handler
        if (command_flag) {
            command_flag = 0; // clear flag
            oi_setWheels(0, 0);
            uart_sendStr("\r\nUser stop command received - halting\r\n");
            return -1; // caller knows user stopped it
        }
         oi_update(sensor_data);
        // --- Bump sensors ---
        if (sensor_data->bumpLeft || sensor_data->bumpRight) {
            oi_setWheels(0, 0);
            uart_sendStr("Bump! Backing up.\r\n");
            move_backward(sensor_data, -currentSpeed / 2.0);
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
            cliffIndex += 1;
            xCliffCoords[cliffIndex] = distance * cos(angle); // assumed in degree mode.
            yCliffCoords[cliffIndex] = distance * sin(angle); // assumed in degree mode.
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
        for (x = 0; x < xCliffCoords.length; ++x) {
            if (abs(currentX - xCliffCoords[x]) <= 15.0) { // or 150 for 15 centimeters.
                xClose = 1;
            } else {
                continue;
            }
        }

        for (x = 0; x < xObjectCoords.length; ++x) {
                if (abs(currentX - xObjectCoords[x]) <= 15.0) { //  or 150 for 15 centimeters.
                    xClose = 1;
                } else {
                    continue;
                }

        }

        for (y = 0; y < yCliffCoords.length; ++y) {
                if (abs(currentY - xCliffCoords[y]) <= 15.0) { // or 150 for 15 centimeters.
                yClose = 1;
            } else {
                continue;
            }
        }
        for (y = 0; y < yObjectCoords.length; ++y) {
                if (abs(currentY - yObjectCoords[y]) <= 15.0) { //  or 150 for 15 centimeters.
                    yClose = 1;
                } else {
                    continue;
                }
        }

        if (xClose == 1 || yClose == 1) { 
            // if it is close in any dimension, then you are going to go out of bounds or run into a previously
            // recorded object. 

            // do similar code to the above if statement.
            // make it stop, and scan for alt. routes.
            // If no alt. routes (stuck), then switch to manual mode.
            // print out a message that says " It's Stuck1 Botmaxxers, switch to Manual Mode!"
            // press a button a keyboard. Make it simple.

        }

        //Adds to the sum for the while condition
        sum += sensor_data -> distance;
        

    }
    //stops the wheels
    oi_setWheels(0, 0);
    currentX = currentX + (sum * cos(currentAngle));
    currentY = currentY + (sum * sin(currentAngle));
     // two lines above update current position of the CyBot.
    return bumped;

}

void move_backward(oi_t *sensor_data, double distance_mm){

    lcd_init();

    double sum = 0;

    oi_setWheels(-150, -150);

    while(sum > distance_mm){

        oi_update(sensor_data);

        sum += sensor_data -> distance;

        lcd_printf("%.2f", sum);

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
        current += sensor_data -> angle; // angles is stored in radians.
        current_degrees = current / .324056; // turns it into degrees. gets moved degrees since last?
        lcd_printf("%.2f", current_degrees);

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

        current += sensor_data -> angle;
        current_degrees = current / .324056;
        lcd_printf("%.2f", current_degrees);
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

    int returned = drive_to_smallest(sensor_data);

    if(returned == 1){

        return 1;

    }

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


                // when stopping an object, you need to add in xy cords of objects that has been detected.
		        objectCoordIndex = objectCoordIndex + 1;
		        xObjectCoords[objectCoordIndex] = fin_distance * (cos(fin_angle)); // uses geometry in order to do it.
		        yObjectCoords[objectCoordIndex] = fin_distance * (sin(fin_angle));



                arrayCounter += 1;
                send_angle_and_dist_object(objectNum, middle_angle, average_distance, angular_width);
            }
        }
    }
}

/*


The below code is idea code. Some of it is included above.


Main() method:

int destination.

while(destination != 1):
	// start.
	oi_moveForward(current_speed, current_speed); // or hardcoded value.
	// The destination function will run everything else.
	
} // Overall, the main method should contain only a few lines.

public int destination() { // 0 if still going, 1 if stopped at destination.
	// First, run autonomously.
	
		
	// 

}




    oi_update(sensor_data);

    if (sensor_data->bumpLeft || sensor_data->bumpRight) {
        // stop or reverse
        oi_setWheels(0, 0);
    }
    if (sensor_data->cliffLeft || sensor_data->cliffFrontLeft ||
        sensor_data->cliffFrontRight || sensor_data->cliffRight) {
        // stop at edge
        oi_setWheels(0, 0);
    }
}

oi_free(sensor_data); // cleanup at end

 // uart_interrupt.c file
volatile int current_speed = 200; // default speed, matches your move_forward(
// uart_interrupt.h file
extern volatile int current_speed;

if (byte_received == '+' || byte_received == '=') {
    current_speed += 50;
    if (current_speed > 500) current_speed = 500; // max speed cap
}
else if (byte_received == '-') {
    current_speed -= 50;
    if (current_speed < 50) current_speed = 50;  // min speed cap
}
else if (byte_received == 'w') { command_byte = 'w'; command_flag = 1; }
else if (byte_received == 's') { command_byte = 's'; command_flag = 1; }
else if (byte_received == 'a') { command_byte = 'a'; command_flag = 1; }
else if (byte_received == 'd') { command_byte = 'd'; command_flag = 1; }
else if (byte_received == 'q') { command_byte = 'q'; command_flag = 1; } // stop

"Important: Don't put oi_setWheels() directly inside the ISR — that's time-consuming code inside an interrupt, which your own comments warn against. Just set flags and let main() act on them."

Update movement.c to use current_speed instead of hardcoded values.

Manual mode loop in main(): 
uart_interrupt_init();
oi_t *sensor_data = oi_alloc();
oi_init(sensor_data);

char speed_str[32];

uart_sendStr("Controls: W/S/A/D=move, +/-=speed, Q=stop\r\n");

while(1) {
    if (command_flag) {
        command_flag = 0; // clear flag first

        if (command_byte == 'w') {
            oi_setWheels(current_speed, current_speed);
        }
        else if (command_byte == 's') {
            oi_setWheels(-current_speed, -current_speed);
        }
        else if (command_byte == 'a') {
            oi_setWheels(-current_speed, current_speed); // turn left
        }
        else if (command_byte == 'd') {
            oi_setWheels(current_speed, -current_speed); // turn right
        }
        else if (command_byte == 'q') {
            oi_setWheels(0, 0);
        }

        // print current speed to PuTTY
        sprintf(speed_str, "\r\nSpeed: %d\r\n", current_speed);
        uart_sendStr(speed_str);
    }

    // still check cliff/bump sensors even in manual mode
    oi_update(sensor_data);
    if (sensor_data->cliffLeft || sensor_data->cliffFrontLeft ||
        sensor_data->cliffFrontRight || sensor_data->cliffRight) {
        oi_setWheels(0, 0);
        uart_sendStr("CLIFF DETECTED - stopped\r\n");
	boundaries[boundariesIndex++] = sensor_data->distance. // or variable that represents current xCoord of cybot, like xCurrent;
	// add to the float boundaries array so CyBot does not come close to it again.
	
    }
}




UART And Scan Controls.c. This file is responsible for the scanning, detection, and avoidance of objects in Lab 7. Lots of edits are going to be made here
in order to do autonomous mode that we desire.

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
           /* if(array_pos < 3){

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
                
		// when stopping an object, you need to add in xy cords of objects that has been detected.
		objectCoordIndex = objectCoordIndex + 1;
		xObjectCoords[objectCoordIndex] = fin_distance * (cos(fin_angle)); // uses geometry in order to do it.
		yObjectCoords[objectCoordIndex] = fin_distance * (sin(fin_angle));
		
                arrayCounter += 1;
                send_angle_and_dist_object(objectNum, middle_angle, average_distance, angular_width);


            }


        }

    }

}






*/
