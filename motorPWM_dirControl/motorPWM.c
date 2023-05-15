/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "motorPWM.pio.h"

const bool CW = true;
const bool CCW = false;
const bool HIGH = true;
const bool LOW = false;
const uint FREQ_HZ = 20000;
const uint DUTY_CYCLE = 30;

struct DC_MOTOR
{
    uint freq; //in hz
    uint dutyCycle; //percentage 0-100
    bool dir;
    uint R_ENABLE_PIN;
    uint L_ENABLE_PIN;
    uint R_PWM_PIN;
    uint L_PWM_PIN;
    uint DIR_PIN;
    uint LOWEST_PWM;
    
    PIO PIO_NUM;
    uint SM_NUM;
    
    uint32_t period;
    uint32_t level;
};

struct DC_MOTOR dcMotor_1;

void setupGPIO(struct DC_MOTOR *motor, uint p1, uint p2, uint p3, uint p4, uint p5)
{
    motor->R_ENABLE_PIN = p1;
    motor->L_ENABLE_PIN = p2;
    motor->R_PWM_PIN = p3;
    motor->L_PWM_PIN = p4;
    motor->DIR_PIN = p5;

    motor->LOWEST_PWM = motor->R_PWM_PIN < motor->L_PWM_PIN ? motor->R_PWM_PIN:motor->L_PWM_PIN;

    gpio_init(motor->R_ENABLE_PIN);
    gpio_init(motor->L_ENABLE_PIN);
    gpio_init(motor->R_PWM_PIN);
    gpio_init(motor->L_PWM_PIN);
    gpio_init(motor->DIR_PIN);

    gpio_set_dir(motor->R_ENABLE_PIN, GPIO_OUT);
    gpio_set_dir(motor->L_ENABLE_PIN, GPIO_OUT);
    gpio_set_dir(motor->R_PWM_PIN, GPIO_OUT);
    gpio_set_dir(motor->L_PWM_PIN, GPIO_OUT);
    gpio_set_dir(motor->DIR_PIN, GPIO_OUT);

    gpio_put(motor->R_ENABLE_PIN, LOW);
    gpio_put(motor->L_ENABLE_PIN, LOW);
    gpio_put(motor->R_PWM_PIN, LOW);
    gpio_put(motor->L_PWM_PIN, LOW);
    gpio_put(motor->DIR_PIN, CW);
}

void initMotor(struct DC_MOTOR *motor, uint freq, uint duty, PIO pio) {
    motor->freq = freq;
    motor->dutyCycle = duty;
    motor->PIO_NUM = pio;

    motor->period = (clock_get_hz(clk_sys) / (uint32_t) motor->freq) / 3 - 3;
    motor->level = motor->period * motor->dutyCycle / 100U;
    motor->dir = -1;

    printf("sys clock: %d\n", clock_get_hz(clk_sys));
    printf("clock divs: %d\n", motor->period);
    printf("clock duty: %d\n", motor->level);
}

void setupPIO(struct DC_MOTOR *motor) {

    // Get first free state machine in PIO 0
    motor->SM_NUM = pio_claim_unused_sm(motor->PIO_NUM, true);
    // Add PIO program to PIO instruction memory. SDK will find location and
    // return with the memory offset of the program.
    uint offset = pio_add_program(motor->PIO_NUM, &motorPWM_program);
    printf("Loaded PIO program at %d\n", offset);
    motorPWM_program_init(motor->PIO_NUM, motor->SM_NUM, offset, motor->LOWEST_PWM, motor->DIR_PIN);
}


// Write `period` to the input shift register
void motorPWM_set_period(struct DC_MOTOR motor) {
    pio_sm_set_enabled(motor.PIO_NUM, motor.SM_NUM, false);
    pio_sm_drain_tx_fifo(motor.PIO_NUM, motor.SM_NUM);
    pio_sm_put_blocking(motor.PIO_NUM, motor.SM_NUM, motor.period);
    pio_sm_exec(motor.PIO_NUM, motor.SM_NUM, pio_encode_pull(false, false));
    pio_sm_exec(motor.PIO_NUM, motor.SM_NUM, pio_encode_out(pio_isr, 32));
}

// Write `level` to TX FIFO. State machine will copy this into X.
void motorPWM_set_level(struct DC_MOTOR motor) {
    pio_sm_put_blocking(motor.PIO_NUM, motor.SM_NUM, motor.level);
}

void stopMotor(struct DC_MOTOR motor) {
    printf("STOPPING MOTOR\n");
    gpio_put(motor.R_ENABLE_PIN, LOW);
    gpio_put(motor.L_ENABLE_PIN, LOW);
    pio_sm_set_enabled(motor.PIO_NUM, motor.SM_NUM, false);
    sleep_ms(1000);
}

void setMotorDir(struct DC_MOTOR motor, bool dir) {
    printf("CHANGING MOTOR DIRECTION to %s\n", dir ? "CW":"CCW");
    gpio_put(motor.DIR_PIN, dir);
    sleep_ms(1000);
}

void runMotor(struct DC_MOTOR *motor, bool dir) {
    printf("RUNNING MOTOR\n");
    if (dir != motor->dir || motor->dir < 0) {
        stopMotor(*motor);
        setMotorDir(*motor, dir);
        motor->dir = dir;
    }
    gpio_put(motor->R_ENABLE_PIN, HIGH);
    gpio_put(motor->L_ENABLE_PIN, HIGH);
    pio_sm_set_enabled(motor->PIO_NUM, motor->SM_NUM, true);
}


void init() {
    stdio_init_all();
    sleep_ms(2000);
    setupGPIO(&dcMotor_1, 21, 20, 19, 18, 17);
    initMotor(&dcMotor_1, FREQ_HZ, DUTY_CYCLE, pio0);
    printf("DC MOTOR 1 PINS \n RE: %d, LE: %d, RP: %d, LP: %d\n", dcMotor_1.R_ENABLE_PIN, dcMotor_1.L_ENABLE_PIN, dcMotor_1.R_PWM_PIN, dcMotor_1.L_PWM_PIN);
    setupPIO(&dcMotor_1);
    printf("motor 1 set with freq: %d, duty: %d, on pin: %d\n", dcMotor_1.freq, dcMotor_1.dutyCycle, dcMotor_1.R_PWM_PIN);
    motorPWM_set_period(dcMotor_1);
    motorPWM_set_level(dcMotor_1);
    // setMotorDir(dcMotor_1, CW);
    //runMotor(&dcMotor_1, CCW);
}

void loop() {
    runMotor(&dcMotor_1, CW);
    sleep_ms(10000);
    runMotor(&dcMotor_1, CCW);
    sleep_ms(10000);
}

int main()
{
    init();
    while(true) {
        loop();
    }
}
