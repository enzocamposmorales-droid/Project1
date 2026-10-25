#pragma once

#include <Arduino.h>

void motorInit();
void motorForward(uint16_t duty);
void motorBrake();
void motorCoast();