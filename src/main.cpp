#include <Arduino.h>

#include "hal_motor.h"
#include "fsm_control.h"
#include "isr_button.h"

void setup() {
    Serial.begin(115200);

    motorInit();
    emergencyButtonInit();
    fsmInit();

    Serial.println("Sistema inicializado");
}

void loop() {
}