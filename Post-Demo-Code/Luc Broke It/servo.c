/*
 * servo.c
 *
 * Created on: Apr 10, 2026
 * Authors: Andy Knockel and Luc Johnson
 */

#include "servo.h"
#include "Timer.h"
#include "button.h"
#include "lcd.h"

volatile uint32_t right_calibration;
volatile uint32_t left_calibration;

void servo_init(void) {
    SYSCTL_RCGCTIMER_R |= 0x02;
    SYSCTL_RCGCGPIO_R |= 0x02;

    while((SYSCTL_PRTIMER_R & 0x2) == 0) {};
    while((SYSCTL_PRGPIO_R & 0x2) == 0) {};

    GPIO_PORTB_DEN_R   |= 0x20;
    GPIO_PORTB_DIR_R  &= ~0x20;

    GPIO_PORTB_AFSEL_R |= 0x20;
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & ~0xF00000) | 0x700000;

    TIMER1_CTL_R &= ~0x100;
    TIMER1_CFG_R |= 0x4;
    TIMER1_TBMR_R = (TIMER1_TBMR_R & ~0xF) | 0xA;

    TIMER1_CTL_R &= ~0x4000;
    TIMER1_TBPR_R = 0x04;
    TIMER1_TBILR_R = 0xE200;

    //default match makes timer high for 1.5ms which should roughly be 90 degrees
    TIMER1_TBMATCHR_R = 0x8440;
    TIMER1_TBPMR_R = 0x4;

    TIMER1_CTL_R |= 0x100;
}

/*
 * Calibrates the servo motor to work properly from 0 to 180 degrees
 * Make sure to set the variables right_calibration and left_calibration to the corresponding
 * values from the function in your main program
 */
void servo_calibrate(void) {
    uint32_t match_value = 304000; //rough starting point for 0 degrees
    uint8_t button = 0;

    lcd_printf("Button 1 for left, 2 for right\nAdjust until 90 deg right");
    while (button != 4) {
        button = button_getButton();

        TIMER1_TBMATCHR_R = (match_value & 0xFFFF);
        TIMER1_TBPMR_R = (match_value >> 16) & 0xFF;

        timer_waitMillis(100);

        if (button == 1) match_value -= 100;
        else if (button == 2) match_value += 100;
    }

    lcd_printf("%d\nPress button 4 for next val", 320000 - match_value);
    while (button_getButton() != 4) {};

    lcd_printf("Button 1 for left, 2 for right\nAdjust until 90 degrees left");
    timer_waitMillis(1000);

    button = 0;
    while (button != 4) {
        button = button_getButton();

        TIMER1_TBMATCHR_R = (match_value & 0xFFFF);
        TIMER1_TBPMR_R = (match_value >> 16) & 0xFF;

        timer_waitMillis(100);

        if (button == 1) match_value -= 100;
        else if (button == 2) match_value += 100;
    }

    lcd_printf("%d\nPress button 4 to quit", 320000 - match_value);
    while (button_getButton() != 4) {};
    lcd_clear();
}

static uint16_t current_deg = 90;

/*
 * Moves the sensor to the given degree position
 * Make sure to calibrate using servo_calibrate() before using
 */
void servo_move(uint16_t degrees) {
    //Don't let servo go past 180 or 0
    if(degrees > 180) degrees = 180;

    //0 degrees is whatever the right calibration value is, 180 is the left
    //the amount of cycles per degree is the difference between the right and left calibration divided by 181, giving the amount per degree
    float cycles_per_degree = (left_calibration - right_calibration) / 181;
    uint32_t high_pulse_cycles = right_calibration + (uint32_t)(degrees * cycles_per_degree);

    //period is 20ms which is 320,000 cycles
    uint32_t match_value = 320000 - high_pulse_cycles;

    //set the lower 16 bits
    TIMER1_TBMATCHR_R = (match_value & 0xFFFF);
    //set the upper 8 bits
    TIMER1_TBPMR_R = (match_value >> 16) & 0xFF; //shift bits 23-16 to position 7-0 and set the prescale to that value

    //5ms per degree + default delay (we will adjust to see what's needed)
    uint8_t default_delay = 100;
    uint32_t movement_delay = (abs(degrees - current_deg) * 5) + default_delay;
    timer_waitMillis(movement_delay);

    //make current_deg what was just sent
    current_deg = degrees;
}
