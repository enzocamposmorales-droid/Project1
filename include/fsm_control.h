#pragma once

#include <Arduino.h>

enum class State : uint8_t {
    IDLE = 0,
    RUNNING,
    FAULT,
    COUNT
};

enum class Event : uint8_t {
    NONE = 0,
    START,
    BTN_PRESS,
    RESET,
    COUNT
};

void fsmInit();
void dispatchEvent(Event event);

State getCurrentState();

const char* stateToString(State state);
const char* eventToString(Event event);