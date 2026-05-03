/*
 * adc.c
 *
 *  Created on: Mar 27, 2026
 *  Authors: Andy Knockel & Luc Johnson
 */
#include "adc.h"

void adc_init(void) {
    // clocks
    SYSCTL_RCGCADC_R |= 0x01;
    SYSCTL_RCGCGPIO_R |= 0x02;

    //wait for clocks to be ready
    while ((SYSCTL_PRGPIO_R & 0x02) == 0) {}; // wait for GPIO
    while ((SYSCTL_PRADC_R & 0x01) == 0) {}; // wait for ADC

    //initialize port b to take analog input
    GPIO_PORTB_DIR_R &= ~0x10;
    GPIO_PORTB_AFSEL_R |= 0x10;
    GPIO_PORTB_DEN_R &= ~0x10;
    GPIO_PORTB_AMSEL_R |= 0x10;

    // disable ss3 before editing it
    ADC0_ACTSS_R &= ~0x08;

    // set to trigger from software
    ADC0_EMUX_R &= ~0xF000;

    //read from pb4
    ADC0_SSMUX3_R = 10;

    //mark bit 2 as end
    ADC0_SSCTL3_R = 0x06;

    //enable after editing
    ADC0_ACTSS_R |= 0x08;
}

uint16_t adc_read(void) {
    ADC0_PSSI_R = 0x08;
    while ((ADC0_RIS_R & 0x08) == 0) {};
    uint16_t value = ADC0_SSFIFO3_R & 0xFFF;
    ADC0_ISC_R = 0x08;
    return value;
}
