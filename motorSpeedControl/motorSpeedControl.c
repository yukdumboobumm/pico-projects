#define DEBUG
#include "dc_motor.h"
#include "pulseCounter.h"

//global constants
const bool CW = true; 
const bool CCW = false;
const bool HIGH = true;
const bool LOW = false;
const uint FREQ_HZ = 20000;
const uint DUTY_CYCLE = 30;

//global pin constants
////pulse counter pins
const uint PULSE_PIN = 22;
// const uint OUT_PIN = 19;
////dc motor pins
const uint R_ENABLE_PIN = 21;
const uint L_ENABLE_PIN = 20;
const uint R_PWM_PIN = 19;
const uint L_PWM_PIN = 18;

//still using a constant for pionum...
const PIO PIONUM = pio0;

//global instances
PULSE_COUNTER pulse_0;
DC_MOTOR motor_0;

//run our initializations
void init() {
    stdio_init_all();
    sleep_ms(2000);//give us some time to breathe
    initPulseCounter(&pulse_0, PULSE_PIN, INT0);
    initPulsePIO(&pulse_0, PIONUM);
    initMotor(&motor_0, R_ENABLE_PIN, L_ENABLE_PIN, R_PWM_PIN, L_PWM_PIN);//DCMOTOR, R-Enable, L-Enable, R-PWM, L-PWM
    initMotorPWM(&motor_0, FREQ_HZ);
    initMotorPIO(&motor_0, pio0);//would be nice to get first available PIO
    setMotorSpeed(&motor_0, 10);
}

//loop forever
void loop() {
    uint dutyCycle = 10;
    bool dir = CW;
    while(true) {
        clearFlag(&pulse_0);
        runMotor(&motor_0, dir);
        sleep_ms(1000);//let motor reach speed
        startCount(&pulse_0, 100);
        while(checkFlag(&pulse_0) == false) {};
        calcSpeed(&pulse_0);
        printf("Duty Cycle: %d, Speed: %.2f\n", dutyCycle, pulse_0.currentHertz);
        // sleep_ms(5000);
        // stopCount(&pulse_0);
        // stopMotor(&motor_0);
        // calcSpeed(&pulse_1);
        // printf("Speed: %d, HZ: %f, RPM: %f\n", speed, pulse_1.currentHertz, pulse_1.currentRPM);
        // speed = ((speed + 5) % 50); //keep us in the range of 10-45
        // speed = !speed ? 10 : speed;
        // setMotorSpeed(&motor_0, speed);
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

