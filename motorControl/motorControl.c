#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"

const bool CW = true;
const bool CCW = false;
const bool HIGH = true;
const bool LOW = false;

struct DC_MOTOR
{
    int freq = 0;
    int dutyCycle = 0;
    bool dir = CW;
    int R_ENABLE_PIN = 0;
    int L_ENABLE_PIN = 0;
    int R_PWM_PIN = 0;
    int L_PWM_PIN = 0;
    PIO stm_pio = pio0;
    int SM_NUM;
}

struct DC_MOTOR dcMotor_1;

void setupGPIO()
{
    dcMotor_1.R_ENABLE_PIN = 21;
    dcMotor_1.L_ENABLE_PIN = 20;
    dcMotor_1.R_PWM_PIN = 19;
    dcMotor_1.L_PWM_PIN = 18;
    dcMotor_1.SM_NUM = 0;

    dcMotor_1.freq = 20000;
    dcMotor_1.dutyCycle = 10;

    gpio_init(R_ENABLE_PIN);
    gpio_init(L_ENABLE_PIN);
    gpio_init(R_PWM_PIN);
    gpio_init(L_PWM_PIN);

    gpio_set_dir(R_ENABLE_PIN, GPIO_OUT);
    gpio_set_dir(L_ENABLE_PIN, GPIO_OUT);
    gpio_set_dir(R_PWM_PIN, GPIO_OUT);
    gpio_set_dir(L_PWM_PIN, GPIO_OUT);

    gpio.put(R_ENABLE_PIN, LOW);
    gpio.put(L_ENABLE_PIN, LOW);
    gpio.put(R_PWM_PIN, LOW);
    gpio.put(L_PWM_PIN, LOW);
}

void setupPIO() {
    // Choose PIO instance (0 or 1)
    PIO pio = pio0;

    // Get first free state machine in PIO 0
    uint sm = pio_claim_unused_sm(pio, true);

    // Add PIO program to PIO instruction memory. SDK will find location and
    // return with the memory offset of the program.
    uint offset = pio_add_program(pio, &motorPWM);

    // Calculate the PIO clock divider
    float div = (float)clock_get_hz(clk_sys) / dcMotor_1.freq;

    // Initialize the program using the helper function in our .pio file
    motorPWM_init(pio, sm, offset, dcMotor_1.R_ENABLE_PIN, div);

    // Start running our PIO program in the state machine
    pio_sm_set_enabled(pio, sm, true);
}

void init() {
    setupGPIO();
    setupPIO();
}

void loop() {
    ;
}

int main()
{
    init();
    while(true) {
        loop();
    }
}
