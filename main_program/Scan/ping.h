/*
 * ping.h
 *
 *  Created on: Apr 3, 2026
 *      Authors: Andy Knockel, Luc Johnson
 */

#ifndef PING_H_
#define PING_H_

typedef enum {
    LOW,
    HIGH,
    DONE
} state_t;

#include <stdint.h>
#include <stdbool.h>
#include <inc/tm4c123gh6pm.h>
#include "driverlib/interrupt.h"

extern volatile uint32_t g_start_time;
extern volatile uint32_t g_end_time;;
extern volatile state_t g_state; // State of ping echo pulse

/**
 * Initialize ping sensor. Uses PB3 and Timer 3B
 */
void ping_init (void);

/**
 * @brief Trigger the ping sensor
 */
void ping_trigger (void);

/**
 * @brief Timer3B ping ISR
 */
void TIMER3B_Handler(void);

/**
 * @brief Calculate the distance in cm
 *
 * @return Distance in cm
 */
float ping_getDistance (void);

/**
 * @brief Calculate the distance in cm from clock width
 *
 * @return Distance in cm from clock width
 */
float ping_getDistanceFromWidth(uint32_t width);

#endif /* PING_H_ */
