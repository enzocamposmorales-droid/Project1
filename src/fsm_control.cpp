#include "fsm_control.h"
#include "hal_motor.h"

// Estado actual del sistema
static State currentState = State::IDLE;

// Tipo de función utilizada por cada transición
using TransitionHandler = void (*)();


// ---------------------------------------------------------
// Handlers de transición
// ---------------------------------------------------------

static void handleNone()
{
    // No se realiza ninguna acción
}

static void handleStart()
{
    motorForward(700);

    currentState = State::RUNNING;

    Serial.println("[FSM] Motor en marcha");
}

static void handleEmergency()
{
    // Parada segura inmediata del motor
    motorBrake();

    currentState = State::FAULT;

    Serial.println("[FSM] PARO DE EMERGENCIA");
}

static void handleReset()
{
    motorCoast();

    currentState = State::IDLE;

    Serial.println("[FSM] Sistema reiniciado");
}

static void handleInvalid()
{
    Serial.println("[FSM] Transicion no permitida");
}


// ---------------------------------------------------------
// Matriz de transición O(1)
//
// Filas    -> State
// Columnas -> Event
//
//            NONE        START         BTN_PRESS        RESET
// IDLE
// RUNNING
// FAULT
// ---------------------------------------------------------

constexpr TransitionHandler FSM
    [static_cast<uint8_t>(State::COUNT)]
    [static_cast<uint8_t>(Event::COUNT)] =
{
    // State::IDLE
    {
        handleNone,
        handleStart,
        handleEmergency,
        handleInvalid
    },

    // State::RUNNING
    {
        handleNone,
        handleInvalid,
        handleEmergency,
        handleInvalid
    },

    // State::FAULT
    {
        handleNone,
        handleInvalid,
        handleEmergency,
        handleReset
    }
};


// ---------------------------------------------------------
// Inicialización
// ---------------------------------------------------------

void fsmInit()
{
    currentState = State::IDLE;

    motorCoast();

    Serial.println("[FSM] Estado inicial: IDLE");
}


// ---------------------------------------------------------
// Despacho del evento
// ---------------------------------------------------------

void dispatchEvent(Event event)
{
    const uint8_t stateIndex =
        static_cast<uint8_t>(currentState);

    const uint8_t eventIndex =
        static_cast<uint8_t>(event);

    if (stateIndex >= static_cast<uint8_t>(State::COUNT) ||
        eventIndex >= static_cast<uint8_t>(Event::COUNT))
    {
        return;
    }

    TransitionHandler handler =
        FSM[stateIndex][eventIndex];

    handler();
}


// ---------------------------------------------------------
// Consulta de estado
// ---------------------------------------------------------

State getCurrentState()
{
    return currentState;
}


// ---------------------------------------------------------
// Conversión a texto para depuración
// ---------------------------------------------------------

const char* stateToString(State state)
{
    switch (state)
    {
        case State::IDLE:
            return "IDLE";

        case State::RUNNING:
            return "RUNNING";

        case State::FAULT:
            return "FAULT";

        default:
            return "UNKNOWN";
    }
}


const char* eventToString(Event event)
{
    switch (event)
    {
        case Event::NONE:
            return "NONE";

        case Event::START:
            return "START";

        case Event::BTN_PRESS:
            return "BTN_PRESS";

        case Event::RESET:
            return "RESET";

        default:
            return "UNKNOWN";
    }
}