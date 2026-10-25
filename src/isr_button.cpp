#include "isr_button.h"

#include "xtensa/core-macros.h"

// Pin del microinterruptor de emergencia
constexpr uint8_t EMERGENCY_BUTTON_PIN = 33;

// Umbral solicitado por la guía:
// 4 000 000 ciclos para el debounce
constexpr uint32_t DEBOUNCE_CYCLES = 4000000UL;

// Variables compartidas entre ISR y loop()
static volatile bool emergencyEvent = false;
static volatile uint32_t buttonPressCount = 0;

// Última marca de tiempo obtenida desde CCOUNT
static volatile uint32_t isrLastCycle = 0;

// Spinlock para proteger las variables compartidas
static portMUX_TYPE isrMux = portMUX_INITIALIZER_UNLOCKED;


// ---------------------------------------------------------
// Rutina de Servicio de Interrupción
// ---------------------------------------------------------
void IRAM_ATTR emergencyButtonISR()
{
    // Lectura directa del contador de ciclos del procesador
    uint32_t currentCycles = XTHAL_GET_CCOUNT();

    // Resta unsigned: funciona incluso cuando CCOUNT hace rollover
    uint32_t elapsedCycles = currentCycles - isrLastCycle;

    // Filtrado del rebote mecánico
    if (elapsedCycles >= DEBOUNCE_CYCLES)
    {
        isrLastCycle = currentCycles;

        // Región crítica protegida mediante spinlock
        portENTER_CRITICAL_ISR(&isrMux);

        emergencyEvent = true;
        buttonPressCount++;

        portEXIT_CRITICAL_ISR(&isrMux);
    }
}


// ---------------------------------------------------------
// Inicialización del microinterruptor
// ---------------------------------------------------------
void emergencyButtonInit()
{
    // El interruptor está conectado entre IO33 y GND.
    pinMode(EMERGENCY_BUTTON_PIN, INPUT_PULLUP);

    // Al pulsar:
    // HIGH -> LOW
    // por eso utilizamos FALLING.
    attachInterrupt(
        digitalPinToInterrupt(EMERGENCY_BUTTON_PIN),
        emergencyButtonISR,
        FALLING
    );
}


// ---------------------------------------------------------
// Lectura segura del evento desde el contexto normal
// ---------------------------------------------------------
bool fetchEmergencyEvent()
{
    bool eventDetected;

    portENTER_CRITICAL(&isrMux);

    eventDetected = emergencyEvent;

    // Consumimos el evento para no procesarlo repetidamente.
    emergencyEvent = false;

    portEXIT_CRITICAL(&isrMux);

    return eventDetected;
}


// ---------------------------------------------------------
// Lectura segura del contador
// ---------------------------------------------------------
uint32_t fetchButtonPressCount()
{
    uint32_t count;

    portENTER_CRITICAL(&isrMux);

    count = buttonPressCount;

    portEXIT_CRITICAL(&isrMux);

    return count;
}