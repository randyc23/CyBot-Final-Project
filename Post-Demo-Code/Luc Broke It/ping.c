/*
 * ping.c
 *
 *  Created on: Apr 3, 2026
 *      Authors: Andy Knockel, Luc Johnson
 */

#include "ping.h"
#include "Timer.h"
#include "lcd.h"

// Global shared variables
// Use extern declarations in the header file

volatile uint32_t g_start_time = 0;
volatile uint32_t g_end_time = 0;
volatile state_t g_state = LOW; // State of ping echo pulse

void ping_init (void){
    //clocks
    SYSCTL_RCGCTIMER_R |= 0x08;
    SYSCTL_RCGCGPIO_R |= 0x02;

    while((SYSCTL_PRGPIO_R & 0x2) == 0) {};
    while((SYSCTL_PRTIMER_R & 0x8) == 0) {};

    //port b enable and alternate function
    GPIO_PORTB_DEN_R   |= 0x08;
    GPIO_PORTB_DIR_R   &= ~0x08;
    GPIO_PORTB_AFSEL_R |= 0x08;

    //set alternate function to timer
    GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R & ~0xF000) | 0x7000;

    TIMER3_CTL_R &= ~0x100;
    TIMER3_CFG_R |= 0x4;

    //set timer mode to input edge time mode
    TIMER3_TBMR_R &= 0x0; //TODO: if something doesn't work check here
    TIMER3_TBMR_R |= 0x7;

    TIMER3_CTL_R |= 0xC00;

    TIMER3_TBPR_R |= 0xFF; //TODO: if timing is weird check here

    TIMER3_TBILR_R |= 0xFFFF;

    TIMER3_IMR_R |= 0x400;
    TIMER3_ICR_R |= 0x400;

    NVIC_EN1_R |= 0x10;

    //NVIC_PRI9_R |= 0x20;

    IntRegister(INT_TIMER3B, TIMER3B_Handler);

    IntMasterEnable();

    // Configure and enable the timer
    TIMER3_CTL_R |= 0x100;
}

void ping_trigger(void) {
    g_state = LOW;
    // Disable timer and disable timer interrupt
    TIMER3_CTL_R &= ~0x100;
    TIMER3_IMR_R &= ~0x400;
    // Disable alternate function (disconnect timer from port pin)
    GPIO_PORTB_AFSEL_R &= ~0x08;

    // YOUR CODE HERE FOR PING TRIGGER/START PULSE
    GPIO_PORTB_DIR_R |= 0x08;

    GPIO_PORTB_DATA_R &= ~0x8;
    GPIO_PORTB_DATA_R |= 0x08;

    timer_waitMicros(5);

    GPIO_PORTB_DATA_R &= ~0x8;
    GPIO_PORTB_DIR_R   &= ~0x08;

    // Clear an interrupt that may have been erroneously triggered
    TIMER3_ICR_R |= 0x400;

    // Re-enable alternate function, timer interrupt, and timer
    GPIO_PORTB_AFSEL_R |= 0x08;
    TIMER3_IMR_R |= 0x400;
    TIMER3_CTL_R |= 0x100;
}

void TIMER3B_Handler(void){

    if (TIMER3_IMR_R & 0x400) {
        TIMER3_ICR_R |= 0x400;
        switch (g_state) {
            case LOW:
                g_start_time = TIMER3_TBR_R;
                g_state = HIGH;
                break;
            case HIGH:
                g_end_time = TIMER3_TBR_R;
                g_state = DONE;
                break;
        }

    }



  // YOUR CODE HERE
  // As needed, go back to review your interrupt handler code for the UART lab.
  // What are the first lines of code in the ISR? Regardless of the device, interrupt handling
  // includes checking the source of the interrupt and clearing the interrupt status bit.
  // Checking the source: test the MIS bit in the MIS register (is the ISR executing
  // because the input capture event happened and interrupts were enabled for that event?
  // Clearing the interrupt: set the ICR bit (so that same event doesn't trigger another interrupt)
  // The rest of the code in the ISR depends on actions needed when the event happens.

}

float ping_getDistance (void) {

    ping_trigger();
    if (g_start_time > g_end_time) {
        return (((g_start_time - g_end_time) * 6.25e-5) * 34.3) / 2;
    } else {
        return -1; //overflow
    }

}

float ping_getDistanceFromWidth(uint32_t width) {
    return ((width * 6.25e-5) * 34.3) / 2;
}
