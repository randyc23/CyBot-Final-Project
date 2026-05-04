/*
 * adc.h
 *
 *  Created on: Mar 27, 2026
 *  Authors: Andy Knockel & Luc Johnson
 */
#include <stdint.h>
#include <inc/tm4c123gh6pm.h>

#ifndef ADC_H_
#define ADC_H_

void adc_init(void);
uint16_t adc_read(void);

#endif /* ADC_H_ */
