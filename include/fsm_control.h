#pragma once
#include <Arduino.h>

enum class State : uint8_t {
    IDLE,
    RUNNING,
    FAULT,
    COUNT
};

enum class Event : uint8_t {
    NONE,
    START,
    BTN_PRESS,
    RESET,
    COUNT
};

void fsmInit();
State getCurrentState();
void dispatchEvent(Event ev);