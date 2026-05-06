/*
 * scan.h
 *
 *  Created on: Apr 26, 2026
 *      Author: knockand
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "uart-interrupt.h"
#include "button.h"
#include "adc.h"
#include "ping.h"
#include "servo.h"

#ifndef SCAN_H_
#define SCAN_H_

typedef struct {
    uint16_t ir;
    float ping; //TODO: might not need to be float
} scan_read;

void scan_init(void);
void scan_feature_enable(uint8_t config);
void scan_range(uint8_t start_angle, uint8_t end_angle, uint8_t increment, uint8_t num_scans);
void scan(scan_read *scan_vals, uint8_t angle, uint8_t num_scans);

#endif /* SCAN_H_ */
