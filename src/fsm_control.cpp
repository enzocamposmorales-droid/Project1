#include "fsm_control.h"

static State currentState = State::IDLE;

void fsmInit() {
    currentState = State::IDLE;
}

State getCurrentState() {
    return currentState;
}

void dispatchEvent(Event ev) {
}