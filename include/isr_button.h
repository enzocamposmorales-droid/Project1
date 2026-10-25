#pragma once

#include <Arduino.h>

void emergencyButtonInit();

// Devuelve true una sola vez por cada pulsación válida.
bool fetchEmergencyEvent();

// Permite consultar cuántas pulsaciones válidas ha detectado la ISR.
uint32_t fetchButtonPressCount();