#include <Arduino.h>

#include "hal_motor.h"
#include "fsm_control.h"
#include "isr_button.h"

void setup()
{
    Serial.begin(115200);

    motorInit();
    emergencyButtonInit();
    fsmInit();

    Serial.println();
    Serial.println("=== Control Banda Transportadora ===");
    Serial.println("Comandos:");
    Serial.println("  s -> START");
    Serial.println("  r -> RESET");
    Serial.println();
    Serial.print("Estado inicial: ");
    Serial.println(stateToString(getCurrentState()));
}

void loop()
{
    // -----------------------------------------------------
    // 1. El paro de emergencia tiene prioridad máxima
    // -----------------------------------------------------
    if (fetchEmergencyEvent())
    {
        dispatchEvent(Event::BTN_PRESS);

        Serial.print("Pulsaciones validas: ");
        Serial.println(fetchButtonPressCount());

        Serial.print("Estado actual: ");
        Serial.println(stateToString(getCurrentState()));

        // Evita procesar START/RESET en la misma iteración
        return;
    }

    // -----------------------------------------------------
    // 2. Entrada serial no bloqueante para pruebas
    // -----------------------------------------------------
    if (Serial.available() > 0)
    {
        const char command = Serial.read();

        switch (command)
        {
            case 's':
            case 'S':
                dispatchEvent(Event::START);
                break;

            case 'r':
            case 'R':
                dispatchEvent(Event::RESET);
                break;

            default:
                break;
        }

        Serial.print("Estado actual: ");
        Serial.println(stateToString(getCurrentState()));
    }
}