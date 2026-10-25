#include "hal_motor.h"
#include "driver/ledc.h"

// Pines internos del puente H L9110S de la IdeaBoard
constexpr uint8_t MOTOR_A_PIN = 12;
constexpr uint8_t MOTOR_B_PIN = 14;

// Configuración PWM solicitada
constexpr uint32_t PWM_FREQUENCY = 1000;
constexpr uint16_t PWM_MAX_DUTY = 1023;

// Timer y canales LEDC
constexpr ledc_mode_t PWM_SPEED_MODE = LEDC_HIGH_SPEED_MODE;
constexpr ledc_timer_t PWM_TIMER = LEDC_TIMER_0;
constexpr ledc_channel_t MOTOR_A_CHANNEL = LEDC_CHANNEL_0;
constexpr ledc_channel_t MOTOR_B_CHANNEL = LEDC_CHANNEL_1;

static void setDuty(ledc_channel_t channel, uint16_t duty)
{
    if (duty > PWM_MAX_DUTY) {
        duty = PWM_MAX_DUTY;
    }

    ledc_set_duty(PWM_SPEED_MODE, channel, duty);
    ledc_update_duty(PWM_SPEED_MODE, channel);
}

void motorInit()
{
    // Configuración del timer LEDC:
    // 1000 Hz y resolución de 10 bits
    ledc_timer_config_t timerConfig = {};
    timerConfig.speed_mode = PWM_SPEED_MODE;
    timerConfig.duty_resolution = LEDC_TIMER_10_BIT;
    timerConfig.timer_num = PWM_TIMER;
    timerConfig.freq_hz = PWM_FREQUENCY;
    timerConfig.clk_cfg = LEDC_AUTO_CLK;

    ledc_timer_config(&timerConfig);

    // Canal A -> GPIO12
    ledc_channel_config_t channelAConfig = {};
    channelAConfig.gpio_num = MOTOR_A_PIN;
    channelAConfig.speed_mode = PWM_SPEED_MODE;
    channelAConfig.channel = MOTOR_A_CHANNEL;
    channelAConfig.intr_type = LEDC_INTR_DISABLE;
    channelAConfig.timer_sel = PWM_TIMER;
    channelAConfig.duty = 0;
    channelAConfig.hpoint = 0;

    ledc_channel_config(&channelAConfig);

    // Canal B -> GPIO14
    ledc_channel_config_t channelBConfig = {};
    channelBConfig.gpio_num = MOTOR_B_PIN;
    channelBConfig.speed_mode = PWM_SPEED_MODE;
    channelBConfig.channel = MOTOR_B_CHANNEL;
    channelBConfig.intr_type = LEDC_INTR_DISABLE;
    channelBConfig.timer_sel = PWM_TIMER;
    channelBConfig.duty = 0;
    channelBConfig.hpoint = 0;

    ledc_channel_config(&channelBConfig);

    // Estado inicial seguro
    motorCoast();
}

void motorForward(uint16_t duty)
{
    // A recibe PWM y B permanece en LOW
    setDuty(MOTOR_A_CHANNEL, duty);
    setDuty(MOTOR_B_CHANNEL, 0);
}

void motorBrake()
{
    // Frenado dinámico:
    // ambas entradas del puente H al mismo nivel alto
    setDuty(MOTOR_A_CHANNEL, PWM_MAX_DUTY);
    setDuty(MOTOR_B_CHANNEL, PWM_MAX_DUTY);
}

void motorCoast()
{
    // Motor sin excitación:
    // ambas entradas en LOW
    setDuty(MOTOR_A_CHANNEL, 0);
    setDuty(MOTOR_B_CHANNEL, 0);
}