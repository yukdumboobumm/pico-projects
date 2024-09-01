#define DEBUG
#include "dc_motor.h"
#include "pulseCounter.h"

//global constants
const bool CW = false; 
const bool CCW = true;
const bool HIGH = true;
const bool LOW = false;
const uint FREQ_HZ = 32000;
const float SET_SPEED_HZ = 24.0;
const float MAX_DUTY_CYCLE = 50.0;
// const uint DUTY_CYCLE = 30;

// const float kP = 0.5, kI = 0.0, kD = 10.5;
const float kP = 1.25, kI=0.04, kD=0.0;
const float kV_CONSTANT = .8181;

//global pin constants
////pulse counter pins
const uint PULSE_PIN = 15;
// const uint OUT_PIN = 19;
////dc motor pins
const uint R_ENABLE_PIN = 21;
const uint L_ENABLE_PIN = 20;
const uint R_PWM_PIN = 19;
const uint L_PWM_PIN = 18;

//still using a constant for pionum...
const PIO PIONUM = pio0;

//global instancess
PULSE_COUNTER pulse_0;
DC_MOTOR motor_0;

//run our initializations
void init() {
    stdio_init_all();
    sleep_ms(3000);//give us some time to breathe
    printf("starting");
    initPulseCounter(&pulse_0, PULSE_PIN, INT0);
    initPulsePIO(&pulse_0, PIONUM);
    initMotor(&motor_0, R_ENABLE_PIN, L_ENABLE_PIN, R_PWM_PIN, L_PWM_PIN);//DCMOTOR, R-Enable, L-Enable, R-PWM, L-PWM
    initMotorPWM(&motor_0, FREQ_HZ);
    initMotorPIO(&motor_0, PIONUM);//would be nice to get first available PIO
    float speedTest = 15;
    bool countUp = true;
    // while(1) {
    //     runMotorForward(&motor_0, speedTest);
    //     clearFlag(&pulse_0);
    //     startCount(&pulse_0, 80);
    //     while(checkFlag(&pulse_0)==false){};
    //     calcSpeed(&pulse_0);
    //     printf("Duty Cycle\t%.2f\tSpeed\t%.2f\n", speedTest, pulse_0.currentHertz);
    //     if (speedTest<MAX_DUTY_CYCLE && countUp) speedTest++;
    //     else countUp = false;
    //     if (speedTest>15 && !countUp) speedTest --;
    //     else countUp = true;
    // }
}

//loop forever
void loop() {
    static float dutyCycle = SET_SPEED_HZ / kV_CONSTANT;
    static float prevError = 0;
    float measured_speed = 0;
    float integral = 0;
    float derivative = 0;
    float error = 0;
    float corrected_speed = 0;
    float control_effort = 0;
    uint64_t dt_ms;
    while(true) {
        clearFlag(&pulse_0);
        // setMotorSpeed(&motor_0, 35);
        runMotorForward(&motor_0, dutyCycle);
        sleep_ms(1000);//let motor reach speed
        startCount(&pulse_0, SET_SPEED_HZ * 5); //sample @ roughly 10x the motor frequency
        while(checkFlag(&pulse_0) == false) {};
        calcSpeed(&pulse_0);
        measured_speed = pulse_0.currentHertz;
        printf("Duty Cycle\t%.2f\tSpeed\t%.2f\n", dutyCycle, measured_speed);
        dt_ms = pulse_0.timeDiff_us / 1000;
        error = SET_SPEED_HZ - measured_speed;
        integral += (error * dt_ms/1000/1000);
        derivative = (error - prevError)/dt_ms;
        prevError = error;
        printf("error: %.2f\tintegral: %.2f\tderivative:%.4f\n",error, integral, derivative);
        control_effort = kP * error + kI * integral + kD * derivative;
        corrected_speed = control_effort + (dutyCycle * kV_CONSTANT);
        dutyCycle = corrected_speed / kV_CONSTANT;
        printf("effort: %.2f\tcorrected: %.2f\tduty: %.2f\n",control_effort,corrected_speed,dutyCycle);
        if (dutyCycle>MAX_DUTY_CYCLE) dutyCycle=MAX_DUTY_CYCLE;
        else if (dutyCycle < 15) dutyCycle = 15;
    }

    // while(true) {
    //     uint counter = 0;
    //     clearFlag(&pulse_0);
    //     startCount(&pulse_0, 100);
    //     // test pulse of 20Hz
    //     while(checkFlag(&pulse_0) == false) {
    //         gpio_put(OUT_PIN, true);
    //         sleep_us(25);
    //         gpio_put(OUT_PIN, false);
    //         sleep_us(50000-25);
    //         counter++;
    //     }
    //     printf("asked for %d, and sent: %d\n",100u, counter);
    //     calcSpeed(&pulse_0);
    //     sleep_ms(1000);
    // }
}

int main() {
    // sleep_ms(2000);//give us some time to breathe
    // stdio_init_all();
    // printf("hello world\n");
    init();
    loop();
}

